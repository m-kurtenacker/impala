#include "type.h"

#include <iostream>

using namespace anydsl2;

int TypeVar::counter = 0;

//------------------------------------------------------------------------------

size_t Type::hash() const {
    // TODO take type variables of generic types better into the equation
    // TODO perhaps store this hash so it does not need to be recomputed all the time
    size_t seed = hash_combine(hash_value((int) kind()), size());
    seed = hash_combine(seed, num_bound_vars());
    for (auto elem : elems())
        seed = hash_combine(seed, elem->hash());

    return seed;
}

bool Type::equal(const Type* other) const {
    if (this->get_representative() == other->get_representative()) {
        return true;
    } else if (this->is_unified() && other->is_unified()) {
        return false;
    }

    bool result = this->kind() == other->kind();
    result &= this->size() == other->size();
    result &= this->num_bound_vars() == other->num_bound_vars();

    // set equivalence constraints for type variables
    for (size_t i = 0, e = num_bound_vars(); i != e; ++i) {
        this->bound_var(i)->set_equiv_variable(other->bound_var(i));
    }

    for (size_t i = 0, e = size(); i != e && result; ++i) {
        result &= this->elem(i)->equal(other->elem(i));
    }

    // unset equivalence constraints for type variables
    for (size_t i = 0, e = num_bound_vars(); i != e; ++i) {
        this->bound_var(i)->unset_equiv_variable();
    }

    return result;
}

bool Type::is_closed() const {
    for (auto t : elems_) {
        if (! t->is_closed()) {
            return false;
        }
    }
    return true;
}

std::string Type::bound_vars_to_string() const {
    std::string result;

    if (!is_generic())
        return result;

    const char* separator = "<";
    for (auto v : bound_vars()) {
        result += separator + v->to_string();
        separator = ",";
    }
    return result + '>';
}

void Type::dump() const { std::cout << to_string() << std::endl; }

//------------------------------------------------------------------------------

std::string PrimType::to_string() const {
    switch (primtype_kind()) {
#define PRIMTYPE(T) case PrimType_##T: return #T;
#include "primtypes.h"
        default: ANYDSL2_UNREACHABLE;
    }
}

std::string CompoundType::elems_to_string() const {
    std::string result;

    if (is_empty())
        return result;

    const char* separator = "(";
    for (auto elem : elems()) {
        result += separator + elem->to_string();
        separator = ", ";
    }
    return result + ')';
}

bool TypeVar::equal(const Type* other) const {
    // TODO is this correct for a instanceof-equivalent?
    if (const TypeVar* t = other->isa<TypeVar>()) {
        // we do not use && because for performance reasons we only set the
        // equiv_var on one side (even the right side of the || should never
        // be executed)
        return (this->equiv_var_ == t) || (t->equiv_var_ == this);
    }
    return false;
}

std::string TypeVar::to_string() const {
    if (id_ < 26) {
        return std::string(1, 'A' + id_);
    } else {
        return std::string("Z") + std::to_string(id_);
    }
}

//------------------------------------------------------------------------------

TypeTable::TypeTable()
    : types_()
#define PRIMTYPE(T) , T##_(unify(new PrimType(*this, PrimType_##T)))
#include "primtypes.h"
    , type_error_(unify(new TypeError(*this)))
{}

const FnType* TypeTable::fntype_simple(TypeArray params, const Type* return_type) {
    const FnType* retfun = fntype( { return_type });

    size_t psize = params.size();

    const Type** p = new const Type*[psize + 1];

    for (int i = 0; i < psize; ++i) {
        p[i] = params[i];
    }
    p[psize] = retfun;

    return fntype(TypeArray(p, psize + 1));
}

void TypeTable::insert_new(const Type* type) {
    assert(!type->is_unified());

    for (auto elem : type->elems()) {
        if (!elem->is_unified()) {
            insert_new(elem);
        }
    }

    type->set_representative(type);

    // TODO is this a correct instanceof test? -- maybe use a virtual method instead?
    if (!type->isa<TypeVar>()) {
        auto p = types_.insert(type);
        assert(p.second && "hash/equal broken");
    }
}

void TypeTable::change_repr(const Type* t, const Type* repr) const {
    assert(repr->is_final_representative());

    if (t->is_unified()) {
        assert(t->get_representative() == repr);
        return;
    }

    assert(t->size() == repr->size());
    for (size_t i = 0, e = t->size(); i != e; ++i) {
        change_repr(t->elem(i), repr->elem(i));
    }

    t->set_representative(repr);
}

const Type* TypeTable::unify_base(const Type* type) {
    // unify only closed types (i.e. only types where all type variables have been bound)
    if (! type->is_closed()) {
        return type;
    }

    assert(!type->is_unified());

    auto i = types_.find(type);

    if (i != types_.end()) {
        if (*i != type) {
            // TODO reset the representative of all not-unified (sub-)types
            assert((*i)->is_final_representative());

            change_repr(type, *i);
        }
        return *i;
    }

    insert_new(type);

    assert(type->is_unified());
    return type;
}

const PrimType* TypeTable::primtype(const PrimTypeKind kind) {
    switch (kind) {
#define PRIMTYPE(T) case PrimType_##T: return T##_;
#include "primtypes.h"
        default: ANYDSL2_UNREACHABLE;
    }
}

void TypeTable::check_sanity() const {
    for (auto t : types_) {
        // TODO assert(t->is_sane()); if t->is_sane() is implemented
        assert(t->is_final_representative());
    }
}

//------------------------------------------------------------------------------

void check_sanity(TypeArray types) {
    for (auto t1 : types) {
        for (auto t2 : types) {
            if (t1->is_unified() && t2->is_unified()) {
                assert((!t1->equal(t2)) || (t1->get_representative() == t2->get_representative()));
            }
        }
    }
}
