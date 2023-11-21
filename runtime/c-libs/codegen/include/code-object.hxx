/*! \file code-object.hxx
 *
 * An abstract interface to mediate between the object files generated by
 * LLVM and the SML/NJ in-memory code objects.
 *
 * \author John Reppy
 */

/*
 * COPYRIGHT (c) 2021 The Fellowship of SML/NJ (http://www.smlnj.org)
 * All rights reserved.
 */

#ifndef _CODE_OBJECT_HXX_
#define _CODE_OBJECT_HXX_

#include <vector>
#include "llvm/Object/ObjectFile.h"

struct target_info;

/// a code-object is container for the parts of an object file that are needed to
/// create the SML code object in the heap.  Its purpose is to abstract from
/// target architecture and object-file format dependencies.  This class is
/// an abstract base class; the actual implementation is specialized to the
/// target.
//
class CodeObject {
  public:

    CodeObject () = delete;
    CodeObject (CodeObject &) = delete;

    virtual ~CodeObject ();

    /// create a code object.
    static std::unique_ptr<CodeObject> create (class code_buffer *codeBuf);

    /// return the size of the code in bytes
    size_t size() const { return this->_szb; }

    /// copy the code into the specified memory, which is assumed to be this->size()
    /// bytes
    void getCode (unsigned char *code);

    /// dump information about the code object to the LLVM debug stream.
    void dump (bool bits);

  protected:
    const target_info *_tgt;
    std::unique_ptr<llvm::object::ObjectFile> _obj;

    /// the size of the heap-allocated code object in bytes
    size_t _szb;

    /// information about a section to be included in the heap-allocated code object
    //
    struct Section {
        llvm::object::SectionRef sect;          ///< the included section
/* FIXME: once we switch to C++17, we can use a std::optional<SectionRef> */
        bool separateRelocSec;                  ///< true if the relocation info for
                                                ///  `sect` is in a separate section
        llvm::object::SectionRef reloc;         ///< a separate section containing the
                                                ///  relocation info for `sect`

        /// constructor
        Section (llvm::object::SectionRef &s) : sect(s), separateRelocSec(false)
        { }

        Section (llvm::object::SectionRef &s, llvm::object::SectionRef &r)
	: sect(s), separateRelocSec(true), reloc(r)
        { }

        llvm::Expected<llvm::StringRef> getName () const
        {
            return this->sect.getName ();
        }
        uint64_t getAddress () const { return this->sect.getAddress (); }
        uint64_t getIndex () const { return this->sect.getIndex (); }
        uint64_t getSize () const { return this->sect.getSize (); }
        llvm::Expected<llvm::StringRef> getContents () const
        {
            return this->sect.getContents ();
        }

        llvm::iterator_range<llvm::object::relocation_iterator> relocations () const
        {
            if (this->separateRelocSec) {
                return this->reloc.relocations ();
            } else {
                return this->sect.relocations ();
            }
        }
        const llvm::object::ObjectFile *getObject () const
        {
            return this->sect.getObject ();
        }

    }; // struct Section

    /// a vector of the sections that are to be included in the heap-allocated code
    /// object.
    std::vector<Section> _sects;

    /// constuctor
    CodeObject (
	target_info const *target,
	std::unique_ptr<llvm::object::ObjectFile> objFile
    ) : _tgt(target), _obj(std::move(objFile)), _szb(0)
    { }

    /// helper function that determines which sections to include and computes
    /// the total size of the SML code object
    // NOTE: because this function invokes the target-specific virtual method
    // `_includeDataSect`, it must be called *after* the object has been
    // constructed.
    //
    void _computeSize ();

    /// should a section be included in the SML data object?
    //
    bool _includeSect (llvm::object::SectionRef const &sect)
    {
	return sect.isText() || (sect.isData() && this->_includeDataSect(sect));
    }

    /// check if a section contains relocation info for an included section?
    /// \param sect  the section being checked
    /// \return an iterator that references the section or `section_end()` for
    ///         the object file
    //
    llvm::object::section_iterator _relocationSect (llvm::object::SectionRef const &sect)
    {
        auto reloc = sect.getRelocatedSection();
        if (reloc
        && (*reloc != this->_obj->section_end())
        && this->_includeSect(**reloc)) {
            return *reloc;
        }
        else {
            return this->_obj->section_end();
        }
    }

    /// should a data section be included in the code object?  This method
    /// is target specific.
    //
    virtual bool _includeDataSect (llvm::object::SectionRef const &sect) = 0;

    /// helper function for resolving relocation records
    //
    virtual void _resolveRelocs (Section &sect, uint8_t *code) = 0;

    /// dump the relocation info for a section
    //
    void _dumpRelocs (llvm::object::SectionRef const &sect);

    /// convert relocation types to strings
    virtual std::string _relocTypeToString (uint64_t ty) = 0;

}; // CodeObject

#endif /// _CODE_OBJECT_HXX_
