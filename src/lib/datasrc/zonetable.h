// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef __ZONETABLE_H
#define __ZONETABLE_H 1

#include <boost/shared_ptr.hpp>

#include <dns/rrset.h>

namespace isc {
namespace dns {
class Name;
class RRClass;
};

namespace datasrc {

/// \brief The base class for a single authoritative zone
///
/// The \c Zone class is an abstract base class for representing
/// a DNS zone as part of data source.
///
/// At the moment this is provided mainly for making the \c ZoneTable class
/// and the authoritative query logic  testable, and only provides a minimal
/// set of features.
/// This is why this class is defined in the same header file, but it may
/// have to move to a separate header file when we understand what is
/// necessary for this class for actual operation.
///
/// The idea is to provide a specific derived zone class for each data
/// source, beginning with in memory one.  At that point the derived classes
/// will have more specific features.  For example, they will maintain
/// information about the location of a zone file, whether it's loaded in
/// memory, etc.
///
/// It's not yet clear how the derived zone classes work with various other
/// data sources when we integrate these components, but one possibility is
/// something like this:
/// - If the underlying database such as some variant of SQL doesn't have an
///   explicit representation of zones (as part of public interface), we can
///   probably use a "default" zone class that simply encapsulates the
///   corresponding data source and calls a common "find" like method.
/// - Some data source may want to specialize it by inheritance as an
///   optimization.  For example, in the current schema design of the sqlite3
///   data source, its (derived) zone class would contain the information of
///   the "zone ID".
///
/// <b>Note:</b> Unlike some other abstract base classes we don't name the
/// class beginning with "Abstract".  This is because we want to have
/// commonly used definitions such as \c Result and \c ZonePtr, and we want
/// to make them look more intuitive.
class Zone {
public:
    /// Result codes of the \c find() method.
    ///
    /// Note: the codes are tentative.  We may need more, or we may find
    /// some of them unnecessary as we implement more details.
    enum Result {
        SUCCESS,                ///< An exact match is found.
        DELEGATION,             ///< The search encounters a zone cut.
        NXDOMAIN, ///< There is no domain name that matches the search name
        NXRRSET,  ///< There is a matching name but no RRset of the search type
        CNAME,    ///< The search encounters and returns a CNAME RR
        DNAME     ///< The search encounters and returns a DNAME RR
    };

    /// A helper structure to represent the search result of \c find().
    ///
    /// This is a straightforward tuple of the result code and a pointer
    /// to the found RRset to represent the result of \c find()
    /// (there will be more members in the future - see the class
    /// description).
    /// We use this in order to avoid overloading the return value for both
    /// the result code ("success" or "not found") and the found object,
    /// i.e., avoid using \c NULL to mean "not found", etc.
    ///
    /// This is a simple value class whose internal state never changes,
    /// so for convenience we allow the applications to refer to the members
    /// directly.
    ///
    /// Note: we should eventually include a notion of "zone node", which
    /// corresponds to a particular domain name of the zone, so that we can
    /// find RRsets of a different RR type for that name (e.g. for type ANY
    /// query or to include DS RRs with delegation).
    ///
    /// Note: we may also want to include the closest enclosure "node" to
    /// optimize including the NSEC for no-wildcard proof (FWIW NSD does that).
    struct FindResult {
        FindResult(Result param_code,
                   const isc::dns::ConstRRsetPtr param_rrset) :
            code(param_code), rrset(param_rrset)
        {}
        const Result code;
        const isc::dns::ConstRRsetPtr rrset;
    };

    ///
    /// \name Constructors and Destructor.
    ///
    //@{
protected:
    /// The default constructor.
    ///
    /// This is intentionally defined as \c protected as this base class should
    /// never be instantiated (except as part of a derived class).
    Zone() {}
public:
    /// The destructor.
    virtual ~Zone() {}
    //@}

    ///
    /// \name Getter Methods
    ///
    /// These methods should never throw an exception.
    //@{
    /// Return the origin name of the zone.
    virtual const isc::dns::Name& getOrigin() const = 0;

    /// Return the RR class of the zone.
    virtual const isc::dns::RRClass& getClass() const = 0;
    //@}

    ///
    /// \name Search Method
    ///
    //@{
    /// Search the zone for a given pair of domain name and RR type.
    ///
    /// Each derived version of this method searches the underlying backend
    /// for the data that best matches the given name and type.
    /// This method is expected to be "intelligent", and identifies the
    /// best possible answer for the search key.  Specifically,
    /// - If the search name belongs under a zone cut, it returns the code
    ///   of \c DELEGATION and the NS RRset at the zone cut.
    /// - If there is no matching name, it returns the code of \c NXDOMAIN,
    ///   and, if DNSSEC is requested, the NSEC RRset that proves the
    ///   non-existence.
    /// - If there is a matching name but no RRset of the search type, it
    ///   returns the code of \c NXRRSET, and, if DNSSEC is required,
    ///   the NSEC RRset for that name.
    /// - If there is a matching name with CNAME, it returns the code of
    ///   \c CNAME and that CNAME RR.
    /// - If the search name matches a delegation point of DNAME, it returns
    ///   the code of \c DNAME and that DNAME RR.
    ///
    /// A derived version of this method may involve internal resource
    /// allocation, especially for constructing the resulting RRset, and may
    /// throw an exception if it fails.
    /// It should not throw other types of exceptions.
    ///
    /// Note: It's quite likely that we'll need to specify search options.
    /// For example, we should be able to specify whether to allow returning
    /// glue records at or under a zone cut.  We leave this interface open
    /// at this moment.
    ///
    /// \param name The domain name to be searched for.
    /// \param type The RR type to be searched for.
    /// \return A \c FindResult object enclosing the search result (see above).
    virtual FindResult find(const isc::dns::Name& name,
                            const isc::dns::RRType& type) const = 0;
    //@}
};

/// \brief A pointer-like type pointing to a \c Zone object.
typedef boost::shared_ptr<Zone> ZonePtr;

/// \brief A pointer-like type pointing to a \c Zone object.
typedef boost::shared_ptr<const Zone> ConstZonePtr;

/// A derived zone class intended to be used with the memory data source.
///
/// Currently this is almost empty and is only used for testing the
/// \c ZoneTable class.  It will be substantially expanded, and will probably
/// moved to a separate header file.
class MemoryZone : public Zone {
    ///
    /// \name Constructors and Destructor.
    ///
    /// \b Note:
    /// The copy constructor and the assignment operator are intentionally
    /// defined as private, making this class non copyable.
    //@{
private:
    MemoryZone(const MemoryZone& source);
    MemoryZone& operator=(const MemoryZone& source);
public:
    /// \brief Constructor from zone parameters.
    ///
    /// This constructor internally involves resource allocation, and if
    /// it fails, a corresponding standard exception will be thrown.
    /// It never throws an exception otherwise.
    ///
    /// \param rrclass The RR class of the zone.
    /// \param origin The origin name of the zone.
    MemoryZone(const isc::dns::RRClass& rrclass, const isc::dns::Name& origin);

    /// The destructor.
    virtual ~MemoryZone();
    //@}

    virtual const isc::dns::Name& getOrigin() const;
    virtual const isc::dns::RRClass& getClass() const;
    virtual FindResult find(const isc::dns::Name& name,
                            const isc::dns::RRType& type) const;

private:
    struct MemoryZoneImpl;
    MemoryZoneImpl* impl_;
};

/// \brief A set of authoritative zones.
///
/// The \c ZoneTable class represents a set of zones of the same RR class
/// and provides a basic interface to help DNS lookup processing.
/// For a given domain name, its \c find() method searches the set for a zone
/// that gives a longest match against that name.
///
/// The set of zones are assumed to be of the same RR class, but the
/// \c ZoneTable class does not enforce the assumption through its interface.
/// For example, the \c add() method does not check if the new zone
/// is of the same RR class as that of the others already in the table.
/// It is caller's responsibility to ensure this assumption.
///
/// <b>Notes to developer:</b>
///
/// The add() method takes a (Boost) shared pointer because it would be
/// inconvenient to require the caller to maintain the ownership of zones,
/// while it wouldn't be safe to delete unnecessary zones inside the zone
/// table.
///
/// On the other hand, the find() method returns a bare pointer, rather than
/// the shared pointer, in order to minimize the dependency on Boost
/// definitions in our public interfaces.  This means the caller can only
/// refer to the returned object (via the pointer) for a short period.
///  It should be okay for simple lookup purposes, but if we see the need
/// for keeping a \c Zone object for a longer period of context, we may
/// have to revisit this decision.
///
/// Currently, \c FindResult::zone is immutable for safety.
/// In future versions we may want to make it changeable.  For example,
/// we may want to allow configuration update on an existing zone.
///
/// In BIND 9's "zt" module, the equivalent of \c find() has an "option"
/// parameter.  The only defined option is the one to specify the "no exact"
/// mode, and the only purpose of that mode is to prefer a second longest match
/// even if there is an exact match in order to deal with type DS query.
/// This trick may help enhance performance, but it also seems to make the
/// implementation complicated for a very limited, minor case.  So, for now,
/// we don't introduce the special mode, and, since it was the only reason to
/// have search options in BIND 9, our initial implementation doesn't provide
/// a switch for options.
class ZoneTable {
public:
    /// Result codes of various public methods of \c ZoneTable.
    ///
    /// The detailed semantics may differ in different methods.
    /// See the description of specific methods for more details.
    enum Result {
        SUCCESS,  ///< The operation is successful.
        EXIST,    ///< A zone is already stored in \c ZoneTable.
        NOTFOUND, ///< The specified zone is not found in \c ZoneTable.
        PARTIALMATCH ///< \c Only a partial match is found in \c find(). 
    };

    /// \brief A helper structure to represent the search result of
    /// <code>ZoneTable::find()</code>.
    ///
    /// This is a straightforward pair of the result code and a pointer
    /// to the found zone to represent the result of \c find().
    /// We use this in order to avoid overloading the return value for both
    /// the result code ("success" or "not found") and the found object,
    /// i.e., avoid using \c NULL to mean "not found", etc.
    ///
    /// This is a simple value class with no internal state, so for
    /// convenience we allow the applications to refer to the members
    /// directly.
    ///
    /// See the description of \c find() for the semantics of the member
    /// variables.
    struct FindResult {
        FindResult(Result param_code, const Zone* param_zone) :
            code(param_code), zone(param_zone)
        {}
        const Result code;
        const Zone* const zone;
    };

    ///
    /// \name Constructors and Destructor.
    ///
    /// \b Note:
    /// The copy constructor and the assignment operator are intentionally
    /// defined as private, making this class non copyable.
    //@{
private:
    ZoneTable(const ZoneTable& source);
    ZoneTable& operator=(const ZoneTable& source);

public:
    /// Default constructor.
    ///
    /// This constructor internally involves resource allocation, and if
    /// it fails, a corresponding standard exception will be thrown.
    /// It never throws an exception otherwise.
    ZoneTable();

    /// The destructor.
    ~ZoneTable();
    //@}

    /// Add a \c Zone to the \c ZoneTable.
    ///
    /// \c zone must not be associated with a NULL pointer; otherwise
    /// an exception of class \c InvalidParameter will be thrown.
    /// If internal resource allocation fails, a corresponding standard
    /// exception will be thrown.
    /// This method never throws an exception otherwise.
    ///
    /// \param zone A \c Zone object to be added.
    /// \return \c SUCCESS If the zone is successfully added to the zone table.
    /// \return \c EXIST The zone table already stores a zone that has the
    /// same origin.
    Result add(ZonePtr zone);

    /// Remove a \c Zone of the given origin name from the \c ZoneTable.
    ///
    /// This method never throws an exception.
    ///
    /// \param origin The origin name of the zone to be removed.
    /// \return \c SUCCESS If the zone is successfully removed from the
    /// zone table.
    /// \return \c NOTFOUND The zone table does not store the zone that matches
    /// \c origin.
    Result remove(const isc::dns::Name& origin);

    /// Find a \c Zone that best matches the given name in the \c ZoneTable.
    ///
    /// It searches the internal storage for a \c Zone that gives the
    /// longest match against \c name, and returns the result in the
    /// form of a \c FindResult object as follows:
    /// - \c code: The result code of the operation.
    ///   - \c SUCCESS: A zone that gives an exact match is found
    ///   - \c PARTIALMATCH: A zone whose origin is a super domain of
    ///     \c name is found (but there is no exact match)
    ///   - \c NOTFOUND: For all other cases.
    /// - \c zone: A pointer to the found \c Zone object if one is found;
    /// otherwise \c NULL.
    ///
    /// The pointer returned in the \c FindResult object is only valid until
    /// the corresponding zone is removed from the zone table.
    /// The caller must ensure that the zone is held in the zone table while
    /// it needs to refer to it.
    ///
    /// This method never throws an exception.
    ///
    /// \param name A domain name for which the search is performed.
    /// \return A \c FindResult object enclosing the search result (see above).
    FindResult find(const isc::dns::Name& name) const;

private:
    struct ZoneTableImpl;
    ZoneTableImpl* impl_;
};
}
}
#endif  // __ZONETABLE_H

// Local Variables:
// mode: c++
// End: