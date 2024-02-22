/**
 * Utilities for descriptor classes.
 */

// keep these macro definitions outside the guard,
// and synchronized with descriptor_undef.h.

#define	CHECK(r) if(r.Error()) return false;

#define DUMP_FIELD(field, format) \
	Debug("%s" #field "=" format "\n", prefix, (field));

#define DUMP_SUBOBJECT(field) { \
	Debug("%s" #field "=[\n", prefix); \
	(field).DumpFields((std::string(prefix) + "\t").c_str()); \
	Debug("%s]\n", prefix); \
}

#ifndef _DESCRIPTOR_H
#define _DESCRIPTOR_H

#include "log.h"
#include "bitstream.h"
#include <cxxabi.h>

class Descriptor
{
public:
	virtual void DumpFields(const char* prefix) const = 0;

	// for convenience and compatibility with previous code
	void Dump() const { Dump(""); }
	void Dump(const char* prefix) const {
		const char* name = abi::__cxa_demangle(typeid(*this).name(), NULL, NULL, NULL);
		Debug("%s[%s\n", prefix, name);
		DumpFields((std::string(prefix) + "\t").c_str());
		Debug("%s[/%s]\n", prefix, name);
		free((void *)name);
	}
};

#endif /* _DESCRIPTOR_H */
