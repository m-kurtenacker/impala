#ifndef IMPALA_PARSER_H
#define IMPALA_PARSER_H

#include <istream>
#include <string>

namespace impala {

class Scope;
class TypeTable;

const Scope* parse(TypeTable& typetable, std::istream& i, const std::string& filename, bool& result);

} // namespace impala

#endif
