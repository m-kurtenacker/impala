#ifndef IMPALA_AST_H
#define IMPALA_AST_H

#include <vector>

#include "anydsl/util/box.h"
#include "anydsl/util/cast.h"
#include "anydsl/util/location.h"

#include "impala/token.h"

namespace impala {

class Decl;
class Expr;
class Fct;
class Printer;
class Stmt;
class Type;
class Sema;

typedef std::vector<const Decl*> Decls;
typedef std::vector<const Expr*> Exprs;
typedef std::vector<const Fct*>  Fcts;
typedef std::vector<const Stmt*> Stmts;

class ASTNode {
public:

    virtual ~ASTNode() {}

    anydsl::Location loc;

    virtual void check(Sema& sema) = 0;
    virtual void dump(Printer& p) const = 0;

    template<class T> T* as()  { return anydsl::scast<T>(this); }
    template<class T> T* isa() { return anydsl::dcast<T>(this); }
    template<class T> const T* as()  const { return anydsl::scast<T>(this); }
    template<class T> const T* isa() const { return anydsl::dcast<T>(this); }
};

class Prg : public ASTNode {
public:

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

    const Fcts& fcts() const { return fcts_; }

private:

    Fcts fcts_;

    friend class Parser;
};

class Fct : public ASTNode {
public:

    Fct() {}

    anydsl::Symbol symbol() const { return symbol_; }
    const Type* retType() const { return retType_; }
    const Stmt* body() const { return body_; }
    const Decls& params() const { return params_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    void set(const anydsl::Position& pos1, const anydsl::Symbol symbol, const Type* retType, const Stmt* body);

    anydsl::Symbol symbol_;
    Decls params_;
    const Type* retType_;
    const Stmt* body_;

    friend class Parser;
};

class Decl : public ASTNode {
public:

    Decl(const Token& tok, const Type* type);

    anydsl::Symbol symbol() const { return symbol_; }
    const Type* type() const { return type_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    anydsl::Symbol symbol_;
    const Type* type_;
};

//------------------------------------------------------------------------------

class Type : public ASTNode {
public:
};

class PrimType : public Type {
public:

    enum Kind {
#define IMPALA_TYPE(itype, atype) TYPE_##itype = Token:: TYPE_##itype,
#include "impala/tokenlist.h"
    };

    PrimType(const anydsl::Location& loc, Kind kind);

    Kind kind() const { return kind_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    Kind kind_;
};

//------------------------------------------------------------------------------

class Expr : public ASTNode {
protected:

    Exprs args_;
};

class EmptyExpr : public Expr {
public:

    EmptyExpr(const anydsl::Location& loc) {
        this->loc = loc;
    }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;
};

class Literal : public Expr {
public:

    enum Kind {
#define IMPALA_LIT(tok, t) tok = Token:: tok,
#include "impala/tokenlist.h"
        BOOL
    };

    Literal(const anydsl::Location& loc, Kind kind, anydsl::Box value);

    Kind kind() const { return kind_; }
    anydsl::Box value() const { return value_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    Kind kind_;
    anydsl::Box value_;
};

class Id : public Expr {
public:

    Id(const Token& tok);

    anydsl::Symbol symbol() const { return symbol_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    anydsl::Symbol symbol_;
    const Decl* decl_; ///< Declaration of the variable in use.
};

class PrefixExpr : public Expr {
public:

    enum Kind {
#define IMPALA_PREFIX(tok, str, prec) tok = Token:: tok,
#include "impala/tokenlist.h"
    };

    PrefixExpr(const anydsl::Position& pos1, Kind kind, const Expr* right);

    const Expr* right() const { return args_[0]; }

    Kind kind() const { return kind_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    Kind kind_;
};

class InfixExpr : public Expr {
public:

    enum Kind {
#define IMPALA_INFIX_ASGN(tok, str, lprec, rprec) tok = Token:: tok,
#define IMPALA_INFIX(     tok, str, lprec, rprec) tok = Token:: tok,
#include "impala/tokenlist.h"
    };

    InfixExpr(const Expr* left, Kind kind, const Expr* right);

    const Expr* left() const { return args_[0]; }
    const Expr* right() const { return args_[1]; }

    Kind kind() const { return kind_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    Kind kind_;
};

/**
 * Just for expr++ and expr--.
 * For indexing and function calls use IndexExpr or CallExpr, respectively.
 */
class PostfixExpr : public Expr {
public:

    enum Kind {
        INC = Token::INC,
        DEC = Token::DEC
    };

    PostfixExpr(const Expr* left, Kind kind, const anydsl::Position& pos2);

    const Expr* left() const { return args_[0]; }

    Kind kind() const { return kind_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    Kind kind_;
};

//------------------------------------------------------------------------------

class Stmt : public ASTNode {
public:

    virtual bool isEmpty() const { return false; }
};

class ExprStmt : public Stmt {
public:

    ExprStmt(const Expr* expr, const anydsl::Position& pos2);

    const Expr* expr() const { return expr_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    const Expr* expr_;
};

class DeclStmt : public Stmt {
public:

    DeclStmt(const Decl* decl, const Expr* init, const anydsl::Position& pos2);

    const Decl* decl() const { return decl_; }
    const Expr* init() const { return init_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    const Decl* decl_;
    const Expr* init_;
};

class IfElseStmt: public Stmt {
public:

    IfElseStmt(const anydsl::Position& pos1, const Expr* cond, const Stmt* ifStmt, const Stmt* elseStmt);

    const Expr* cond() const { return cond_; }
    const Stmt* ifStmt() const { return ifStmt_; }
    const Stmt* elseStmt() const { return elseStmt_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    const Expr* cond_;
    const Stmt* ifStmt_;
    const Stmt* elseStmt_;
};

class Loop : public Stmt {
public:

    Loop() {}

    const Expr* cond() const { return cond_; }
    const Stmt* body() const { return body_; }

protected:

    void set(const Expr* cond, const Stmt* body) {
        cond_ = cond;
        body_ = body;
    }

private:

    const Expr* cond_;
    const Stmt* body_;
};

class WhileStmt : public Loop {
public:

    WhileStmt() {}
        
    void set(const anydsl::Position& pos1, const Expr* cond, const Stmt* body);

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;
};

class DoWhileStmt : public Loop {
public:

    DoWhileStmt() {}

    void set(const anydsl::Position& pos1, const Stmt* body, const Expr* cond, const anydsl::Position& pos2);

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;
};

class ForStmt : public Loop {
public:

    ForStmt() {}

    void set(const anydsl::Position& pos1, const Expr* cond, const Expr* inc, const Stmt* body);
    void set(const DeclStmt* d) { initDecl_ = d; isDecl_ = true; }
    void set(const ExprStmt* e) { initExpr_ = e; isDecl_ = false; }

    const DeclStmt* initDecl() const { return initDecl_; }
    const ExprStmt* initExpr() const { return initExpr_; }
    const Stmt* init() const { return isDecl_ ? (const Stmt*) initDecl_ : (const Stmt*) initExpr_; }
    const Expr* inc() const { return inc_; }

    bool isDecl() const { return isDecl_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    union {
        const DeclStmt* initDecl_;
        const ExprStmt* initExpr_;
    };

    const Expr* inc_;
    bool isDecl_;
};

class BreakStmt : public Stmt {
public:

    BreakStmt(const anydsl::Position& pos1, const anydsl::Position& pos2, const Loop* loop);

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    const Loop* loop_;
};

class ContinueStmt : public Stmt {
public:

    ContinueStmt(const anydsl::Position& pos1, const anydsl::Position& pos2, const Loop* loop);

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    const Loop* loop_;
};

class ReturnStmt : public Stmt {
public:

    ReturnStmt(const anydsl::Position& pos1, const Expr* expr, const anydsl::Position& pos2);

    const Expr* expr() const { return expr_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;

private:

    const Expr* expr_;
};

class ScopeStmt : public Stmt {
public:

    ScopeStmt() {}
    ScopeStmt(const anydsl::Location& loc) {
        this->loc = loc;
    }

    const Stmts& stmts() const { return stmts_; }

    virtual void check(Sema& sema);
    virtual void dump(Printer& p) const;
    virtual bool isEmpty() { return stmts_.empty(); }

private:

    Stmts stmts_;

    friend class Parser;
};

//------------------------------------------------------------------------------

} // namespace impala

#endif // IMPALA_AST_H
