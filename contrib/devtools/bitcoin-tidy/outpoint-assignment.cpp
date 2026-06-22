#include "outpoint-assignment.h"

#include <clang/AST/ExprObjC.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>

namespace {

using namespace clang;
using namespace clang::ast_matchers;

struct AssignmentInfo {
  MemberExpr const* ME = nullptr;
  Expr const* Base = nullptr;
  Expr const* RHS = nullptr;
  StringRef MemberName;
};

std::optional<AssignmentInfo> GetAssignment(Stmt const* S)
{
  // Built-in assignment.
  if (auto* BO = dyn_cast<BinaryOperator>(S)) {
    if (!BO->isAssignmentOp())
      return std::nullopt;

    auto* ME = dyn_cast<MemberExpr>(BO->getLHS()->IgnoreParenImpCasts());
    if (!ME)
      return std::nullopt;

    auto* FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
    if (!FD || FD->getName() != "n")
      return std::nullopt;

    return AssignmentInfo{
      ME,
      ME->getBase()->IgnoreParenImpCasts(),
      BO->getRHS(),
      FD->getName(),
    };
  }

  if (auto* E = dyn_cast<ExprWithCleanups>(S)) {
    S = E->getSubExpr();
  }

  // Overloaded operator=.
  if (auto* Op = dyn_cast<CXXOperatorCallExpr>(S)) {
    if (Op->getOperator() != OO_Equal)
      return std::nullopt;

    auto* ME = dyn_cast<MemberExpr>(Op->getArg(0)->IgnoreParenImpCasts());
    if (!ME)
      return std::nullopt;

    auto* FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
    if (!FD)
      return std::nullopt;

    return AssignmentInfo{
      ME,
      ME->getBase()->IgnoreParenImpCasts(),
      Op->getArg(1),
      FD->getName(),
    };
  }

  return std::nullopt;
}

}  // namespace

void OutpointAssignment::registerMatchers(MatchFinder* Finder)
{
  Finder->addMatcher(compoundStmt().bind("body"), this);
}

void OutpointAssignment::check(MatchFinder::MatchResult const& Result)
{
  auto const* Body = Result.Nodes.getNodeAs<CompoundStmt>("body");
  if (!Body) {
    return;
  }

  llvm::SmallVector<Stmt const*> Stmts(Body->body());

  for (size_t i = 0; i + 1 < Stmts.size(); ++i) {
    auto A1 = GetAssignment(Stmts[i]);
    auto A2 = GetAssignment(Stmts[i + 1]);

    if (!A1 || !A2) {
      continue;
    }

    bool hashFirst = A1->MemberName == "hash" && A2->MemberName == "n";
    bool nFirst = A1->MemberName == "n" && A2->MemberName == "hash";
    if (!hashFirst && !nFirst) {
      continue;
    }

    auto& SM = *Result.SourceManager;

    CharSourceRange R1 =
      CharSourceRange::getTokenRange(A1->Base->getSourceRange());

    CharSourceRange R2 =
      CharSourceRange::getTokenRange(A2->Base->getSourceRange());

    StringRef Base1 =
      Lexer::getSourceText(R1, SM, Result.Context->getLangOpts());

    StringRef Base2 =
      Lexer::getSourceText(R2, SM, Result.Context->getLangOpts());

    if (Base1 != Base2) {
      continue;
    }

    Expr const* HashExpr = hashFirst ? A1->RHS : A2->RHS;
    Expr const* NExpr = hashFirst ? A2->RHS : A1->RHS;

    std::string HashText =
      Lexer::getSourceText(
        CharSourceRange::getTokenRange(HashExpr->getSourceRange()), SM,
        Result.Context->getLangOpts())
        .str();

    std::string NText =
      Lexer::getSourceText(
        CharSourceRange::getTokenRange(NExpr->getSourceRange()), SM,
        Result.Context->getLangOpts())
        .str();

    std::string Replacement =
      Base1.str() + " = COutPoint(" + HashText + ", " + NText + ")";

    CharSourceRange ReplaceRange = CharSourceRange::getTokenRange(
      SourceRange(Stmts[i]->getBeginLoc(), Stmts[i + 1]->getEndLoc()));

    diag(
      Stmts[i]->getBeginLoc(),
      "replace separate hash/n assignments with aggregate assignment")
      << FixItHint::CreateReplacement(ReplaceRange, Replacement);
  }
}
