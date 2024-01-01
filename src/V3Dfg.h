// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Data flow graph (DFG) representation of logic
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// Copyright 2003-2024 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
//
// This is a data-flow graph based representation of combinational logic,
// the main difference from a V3Graph is that DfgVertex owns the storage
// of it's input edges (operands/sources/arguments), and can access each
// input edge directly by indexing, making modifications more efficient
// than the linked list based structures used by V3Graph.
//
// A bulk of the DfgVertex sub-types are generated by astgen, and are
// analogous to the corresponding AstNode sub-types.
//
// See also the internals documentation docs/internals.rst
//
//*************************************************************************

#ifndef VERILATOR_V3DFG_H_
#define VERILATOR_V3DFG_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"
#include "V3Error.h"
#include "V3Global.h"
#include "V3Hash.h"
#include "V3List.h"
#include "V3ThreadSafety.h"

#include "V3Dfg__gen_forward_class_decls.h"  // From ./astgen

#include <algorithm>
#include <array>
#include <functional>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <vector>

#ifndef VL_NOT_FINAL
#define VL_NOT_FINAL  // This #define fixes broken code folding in the CLion IDE
#endif

class DfgEdge;
class DfgVisitor;

//------------------------------------------------------------------------------

// Specialization of std::hash for a std::pair<const DfgVertex*, const DfgVertex*> for use below
template <>
struct std::hash<std::pair<const DfgVertex*, const DfgVertex*>> final {
    size_t operator()(const std::pair<const DfgVertex*, const DfgVertex*>& item) const {
        const size_t a = reinterpret_cast<std::uintptr_t>(item.first);
        const size_t b = reinterpret_cast<std::uintptr_t>(item.second);
        constexpr size_t halfWidth = 8 * sizeof(b) / 2;
        return a ^ ((b << halfWidth) | (b >> halfWidth));
    }
};

//------------------------------------------------------------------------------
// Dataflow vertex type enum
//------------------------------------------------------------------------------

class VDfgType final {
public:
#include "V3Dfg__gen_type_enum.h"  // From ./astgen
    enum en m_e;
    VDfgType() = default;
    // cppcheck-suppress noExplicitConstructor
    constexpr VDfgType(en _e)
        : m_e{_e} {}
    constexpr operator en() const { return m_e; }
};
constexpr bool operator==(VDfgType lhs, VDfgType rhs) { return lhs.m_e == rhs.m_e; }
constexpr bool operator==(VDfgType lhs, VDfgType::en rhs) { return lhs.m_e == rhs; }
constexpr bool operator==(VDfgType::en lhs, VDfgType rhs) { return lhs == rhs.m_e; }
inline std::ostream& operator<<(std::ostream& os, const VDfgType& t) { return os << t.ascii(); }

//------------------------------------------------------------------------------
// Dataflow graph
//------------------------------------------------------------------------------

class DfgGraph final {
    friend class DfgVertex;

    // TYPES

    // RAII handle for DfgVertex user data
    class UserDataInUse final {
        DfgGraph* m_graphp;  // The referenced graph

    public:
        // cppcheck-suppress noExplicitConstructor
        UserDataInUse(DfgGraph* graphp)
            : m_graphp{graphp} {}
        // cppcheck-suppress noExplicitConstructor
        UserDataInUse(UserDataInUse&& that) {
            UASSERT(that.m_graphp, "Moving from empty");
            m_graphp = vlstd::exchange(that.m_graphp, nullptr);
        }
        VL_UNCOPYABLE(UserDataInUse);
        UserDataInUse& operator=(UserDataInUse&& that) {
            UASSERT(that.m_graphp, "Moving from empty");
            m_graphp = vlstd::exchange(that.m_graphp, nullptr);
            return *this;
        }

        ~UserDataInUse() {
            if (m_graphp) m_graphp->m_userCurrent = 0;
        }
    };

    // MEMBERS

    // Variables and constants make up a significant proportion of vertices (40-50% was observed
    // in large designs), and they can often be treated specially in algorithms, which in turn
    // enables significant Verilation performance gains, so we keep these in separate lists for
    // direct access.
    V3List<DfgVertex*> m_varVertices;  // The variable vertices in the graph
    V3List<DfgVertex*> m_constVertices;  // The constant vertices in the graph
    V3List<DfgVertex*> m_opVertices;  // The operation vertices in the graph

    size_t m_size = 0;  // Number of vertices in the graph
    uint32_t m_userCurrent = 0;  // Vertex user data generation number currently in use
    uint32_t m_userCnt = 0;  // Vertex user data generation counter
    // Parent of the graph (i.e.: the module containing the logic represented by this graph).
    AstModule* const m_modulep;
    const string m_name;  // Name of graph (for debugging)

public:
    // CONSTRUCTOR
    explicit DfgGraph(AstModule& module, const string& name = "") VL_MT_DISABLED;
    ~DfgGraph() VL_MT_DISABLED;
    VL_UNCOPYABLE(DfgGraph);

    // METHODS
public:
    // Add DfgVertex to this graph (assumes not yet contained).
    inline void addVertex(DfgVertex& vtx);
    // Remove DfgVertex form this graph (assumes it is contained).
    inline void removeVertex(DfgVertex& vtx);
    // Number of vertices in this graph
    size_t size() const { return m_size; }
    // Parent module
    AstModule* modulep() const { return m_modulep; }
    // Name of this graph
    const string& name() const { return m_name; }

    // Reset Vertex user data
    UserDataInUse userDataInUse() {
        UASSERT(!m_userCurrent, "Conflicting use of DfgVertex user data");
        ++m_userCnt;
        UASSERT(m_userCnt, "'m_userCnt' overflow");
        m_userCurrent = m_userCnt;
        return UserDataInUse{this};
    }

    // Access to vertex lists for faster iteration in important contexts
    inline DfgVertexVar* varVerticesBeginp() const;
    inline DfgVertexVar* varVerticesRbeginp() const;
    inline DfgConst* constVerticesBeginp() const;
    inline DfgConst* constVerticesRbeginp() const;
    inline DfgVertex* opVerticesBeginp() const;
    inline DfgVertex* opVerticesRbeginp() const;

    // Calls given function 'f' for each vertex in the graph. It is safe to manipulate any vertices
    // in the graph, or to delete/unlink the vertex passed to 'f' during iteration. It is however
    // not safe to delete/unlink any vertex in the same graph other than the one passed to 'f'.
    inline void forEachVertex(std::function<void(DfgVertex&)> f);

    // 'const' variant of 'forEachVertex'. No mutation allowed.
    inline void forEachVertex(std::function<void(const DfgVertex&)> f) const;

    // Add contents of other graph to this graph. Leaves other graph empty.
    void addGraph(DfgGraph& other) VL_MT_DISABLED;

    // Split this graph into individual components (unique sub-graphs with no edges between them).
    // Also removes any vertices that are not weakly connected to any variable.
    // Leaves 'this' graph empty.
    std::vector<std::unique_ptr<DfgGraph>> splitIntoComponents(std::string label) VL_MT_DISABLED;

    // Extract cyclic sub-graphs from 'this' graph. Cyclic sub-graphs are those that contain at
    // least one strongly connected component (SCC) plus any other vertices that feed or sink from
    // the SCCs, up to a variable boundary. This means that the returned graphs are guaranteed to
    // be cyclic, but they are not guaranteed to be strongly connected (however, they are always
    // at least weakly connected). Trivial SCCs that are acyclic (i.e.: vertices that are not part
    // of a cycle) are left in 'this' graph. This means that at the end 'this' graph is guaranteed
    // to be a DAG (acyclic). 'this' will not necessarily be a connected graph at the end, even if
    // it was originally connected.
    std::vector<std::unique_ptr<DfgGraph>>
    extractCyclicComponents(std::string label) VL_MT_DISABLED;

    // Dump graph in Graphviz format into the given stream 'os'. 'label' is added to the name of
    // the graph which is included in the output.
    void dumpDot(std::ostream& os, const string& label = "") const VL_MT_DISABLED;
    // Dump graph in Graphviz format into a new file with the given 'fileName'. 'label' is added to
    // the name of the graph which is included in the output.
    void dumpDotFile(const string& fileName, const string& label = "") const VL_MT_DISABLED;
    // Dump graph in Graphviz format into a new automatically numbered debug file. 'label' is
    // added to the name of the graph, which is included in the file name and the output.
    void dumpDotFilePrefixed(const string& label = "") const VL_MT_DISABLED;
    // Dump upstream (source) logic cone starting from given vertex into a file with the given
    // 'fileName'. 'name' is the name of the graph, which is included in the output.
    void dumpDotUpstreamCone(const string& fileName, const DfgVertex& vtx,
                             const string& name = "") const VL_MT_DISABLED;
    // Dump all individual logic cones driving external variables in Graphviz format into separate
    // new automatically numbered debug files. 'label' is added to the name of the graph, which is
    // included in the file names and the output. This is useful for very large graphs that are
    // otherwise difficult to browse visually due to their size.
    void dumpDotAllVarConesPrefixed(const string& label = "") const VL_MT_DISABLED;
};

//------------------------------------------------------------------------------
// Dataflow graph edge
//------------------------------------------------------------------------------

class DfgEdge final {
    friend class DfgVertex;

    DfgEdge* m_nextp = nullptr;  // Next edge in sink list
    DfgEdge* m_prevp = nullptr;  // Previous edge in sink list
    DfgVertex* m_sourcep = nullptr;  // The source vertex driving this edge
    // Note that the sink vertex owns the edge, so it is immutable, but because we want to be able
    // to allocate these as arrays, we use a default constructor + 'init' method to set m_sinkp.
    DfgVertex* const m_sinkp = nullptr;  // The sink vertex

public:
    DfgEdge() {}
    void init(DfgVertex* sinkp) { const_cast<DfgVertex*&>(m_sinkp) = sinkp; }

    // The source (driver) of this edge
    DfgVertex* sourcep() const { return m_sourcep; }
    // The sink (consumer) of this edge
    DfgVertex* sinkp() const { return m_sinkp; }
    // Remove driver of this edge
    void unlinkSource() VL_MT_DISABLED;
    // Relink this edge to be driven from the given new source vertex
    void relinkSource(DfgVertex* newSourcep) VL_MT_DISABLED;
};

//------------------------------------------------------------------------------
// Dataflow graph vertex
//------------------------------------------------------------------------------

// Base data flow graph vertex
class DfgVertex VL_NOT_FINAL {
    friend class DfgGraph;
    friend class DfgEdge;
    friend class DfgVisitor;

    using UserDataStorage = void*;  // Storage allocated for user data

    // STATE
    V3ListEnt<DfgVertex*> m_verticesEnt;  // V3List handle of this vertex, kept under the DfgGraph
protected:
    DfgEdge* m_sinksp = nullptr;  // List of sinks of this vertex
    FileLine* const m_filelinep;  // Source location
    AstNodeDType* m_dtypep;  // Data type of the result of this vertex - mutable for efficiency
    DfgGraph* m_graphp;  // The containing DfgGraph
    const VDfgType m_type;  // Vertex type tag
    uint32_t m_userCnt = 0;  // User data generation number
    UserDataStorage m_userDataStorage;  // User data storage

    // CONSTRUCTOR
    DfgVertex(DfgGraph& dfg, VDfgType type, FileLine* flp, AstNodeDType* dtypep) VL_MT_DISABLED;

public:
    virtual ~DfgVertex() VL_MT_DISABLED;

    // METHODS
private:
    // Visitor accept method
    virtual void accept(DfgVisitor& v) = 0;

    // Part of Vertex equality only dependent on this vertex
    virtual bool selfEquals(const DfgVertex& that) const VL_MT_DISABLED;

    // Part of Vertex hash only dependent on this vertex
    virtual V3Hash selfHash() const VL_MT_DISABLED;

public:
    // Supported packed types
    static bool isSupportedPackedDType(const AstNodeDType* dtypep) {
        dtypep = dtypep->skipRefp();
        if (const AstBasicDType* const typep = VN_CAST(dtypep, BasicDType)) {
            return typep->keyword().isIntNumeric();
        }
        if (const AstPackArrayDType* const typep = VN_CAST(dtypep, PackArrayDType)) {
            return isSupportedPackedDType(typep->subDTypep());
        }
        if (const AstNodeUOrStructDType* const typep = VN_CAST(dtypep, NodeUOrStructDType)) {
            return typep->packed();
        }
        return false;
    }

    // Returns true if an AstNode with the given 'dtype' can be represented as a DfgVertex
    static bool isSupportedDType(const AstNodeDType* dtypep) {
        dtypep = dtypep->skipRefp();
        // Support unpacked arrays of packed types
        if (const AstUnpackArrayDType* const typep = VN_CAST(dtypep, UnpackArrayDType)) {
            return isSupportedPackedDType(typep->subDTypep());
        }
        // Support packed types
        return isSupportedPackedDType(dtypep);
    }

    // Return data type used to represent any packed value of the given 'width'. All packed types
    // of a given width use the same canonical data type, as the only interesting information is
    // the total width.
    static AstNodeDType* dtypeForWidth(uint32_t width) {
        return v3Global.rootp()->typeTablep()->findLogicDType(width, width, VSigning::UNSIGNED);
    }

    // Return data type used to represent the type of 'nodep' when converted to a DfgVertex
    static AstNodeDType* dtypeFor(const AstNode* nodep) {
        UDEBUGONLY(UASSERT_OBJ(isSupportedDType(nodep->dtypep()), nodep, "Unsupported dtype"););
        // For simplicity, all packed types are represented with a fixed type
        if (AstUnpackArrayDType* const typep = VN_CAST(nodep->dtypep(), UnpackArrayDType)) {
            // TODO: these need interning via AstTypeTable otherwise they leak
            return new AstUnpackArrayDType{typep->fileline(),
                                           dtypeForWidth(typep->subDTypep()->width()),
                                           typep->rangep()->cloneTree(false)};
        }
        return dtypeForWidth(nodep->width());
    }

    // Source location
    FileLine* fileline() const { return m_filelinep; }
    // The data type of the result of the nodes
    AstNodeDType* dtypep() const { return m_dtypep; }
    void dtypep(AstNodeDType* nodep) { m_dtypep = nodep; }
    // The type of this vertex
    VDfgType type() const { return m_type; }

    // Retrieve user data, constructing it fresh on first try.
    template <typename T>
    T& user() {
        static_assert(sizeof(T) <= sizeof(UserDataStorage),
                      "Size of user data type 'T' is too large for allocated storage");
        static_assert(alignof(T) <= alignof(UserDataStorage),
                      "Alignment of user data type 'T' is larger than allocated storage");
        T* const storagep = reinterpret_cast<T*>(&m_userDataStorage);
        const uint32_t userCurrent = m_graphp->m_userCurrent;
        UDEBUGONLY(UASSERT_OBJ(userCurrent, this, "DfgVertex user data used without reserving"););
        if (m_userCnt != userCurrent) {
            m_userCnt = userCurrent;
            // cppcheck-has-bug-suppress uninitvar
            VL_ATTR_UNUSED T* const resultp = new (storagep) T{};
            UDEBUGONLY(UASSERT_OBJ(resultp == storagep, this, "Something is odd"););
        }
        return *storagep;
    }

    // Retrieve user data, must be current.
    template <typename T>
    T& getUser() {
        static_assert(sizeof(T) <= sizeof(UserDataStorage),
                      "Size of user data type 'T' is too large for allocated storage");
        static_assert(alignof(T) <= alignof(UserDataStorage),
                      "Alignment of user data type 'T' is larger than allocated storage");
        T* const storagep = reinterpret_cast<T*>(&m_userDataStorage);
#if VL_DEBUG
        const uint32_t userCurrent = m_graphp->m_userCurrent;
        UASSERT_OBJ(userCurrent, this, "DfgVertex user data used without reserving");
        UASSERT_OBJ(m_userCnt == userCurrent, this, "DfgVertex user data is stale");
#endif
        return *storagep;
    }

    // Set user data, becomes current.
    template <typename T>
    typename std::enable_if<sizeof(T) <= sizeof(void*), void>::type setUser(T value) {
        static_assert(sizeof(T) <= sizeof(UserDataStorage),
                      "Size of user data type 'T' is too large for allocated storage");
        static_assert(alignof(T) <= alignof(UserDataStorage),
                      "Alignment of user data type 'T' is larger than allocated storage");
        T* const storagep = reinterpret_cast<T*>(&m_userDataStorage);
        const uint32_t userCurrent = m_graphp->m_userCurrent;
#if VL_DEBUG
        UASSERT_OBJ(userCurrent, this, "DfgVertex user data used without reserving");
#endif
        m_userCnt = userCurrent;
        *storagep = value;
    }

    // Width of result
    uint32_t width() const {
        // This is a hot enough function that this is an expensive check, so in debug build only.
        UDEBUGONLY(UASSERT_OBJ(VN_IS(dtypep(), BasicDType), this, "non-packed has no 'width()'"););
        return dtypep()->width();
    }

    // Cache type for 'equals' below
    using EqualsCache = std::unordered_map<std::pair<const DfgVertex*, const DfgVertex*>, uint8_t>;

    // Vertex equality (based on this vertex and all upstream vertices feeding into this vertex).
    // Returns true, if the vertices can be substituted for each other without changing the
    // semantics of the logic. The 'cache' argument is used to store results to avoid repeat
    // evaluations, but it requires that the upstream sources of the compared vertices do not
    // change between invocations.
    bool equals(const DfgVertex& that, EqualsCache& cache) const VL_MT_DISABLED;

    // Uncached version of 'equals'
    bool equals(const DfgVertex& that) const {
        EqualsCache cache;  // Still cache recursive calls within this invocation
        return equals(that, cache);
    }

    // Hash of vertex (depends on this vertex and all upstream vertices feeding into this vertex).
    // Uses user data for caching hashes
    V3Hash hash() VL_MT_DISABLED;

    // Source edges of this vertex
    virtual std::pair<DfgEdge*, size_t> sourceEdges() = 0;

    // Source edges of this vertex
    virtual std::pair<const DfgEdge*, size_t> sourceEdges() const = 0;

    // Arity (number of sources) of this vertex
    size_t arity() const { return sourceEdges().second; }

    // Predicate: has 1 or more sinks
    bool hasSinks() const { return m_sinksp != nullptr; }

    // Predicate: has 2 or more sinks
    bool hasMultipleSinks() const { return m_sinksp && m_sinksp->m_nextp; }

    // Fanout (number of sinks) of this vertex (expensive to compute)
    uint32_t fanout() const VL_MT_DISABLED;

    // Unlink from container (graph or builder), then delete this vertex
    void unlinkDelete(DfgGraph& dfg) VL_MT_DISABLED;

    // Relink all sinks to be driven from the given new source
    void replaceWith(DfgVertex* newSourcep) VL_MT_DISABLED;

    // Access to vertex list for faster iteration in important contexts
    DfgVertex* verticesNext() const { return m_verticesEnt.nextp(); }
    DfgVertex* verticesPrev() const { return m_verticesEnt.prevp(); }

    // Calls given function 'f' for each source vertex of this vertex
    // Unconnected source edges are not iterated.
    inline void forEachSource(std::function<void(DfgVertex&)> f) VL_MT_DISABLED;

    // Calls given function 'f' for each source vertex of this vertex
    // Unconnected source edges are not iterated.
    inline void forEachSource(std::function<void(const DfgVertex&)> f) const VL_MT_DISABLED;

    // Calls given function 'f' for each source edge of this vertex. Also passes source index.
    inline void forEachSourceEdge(std::function<void(DfgEdge&, size_t)> f) VL_MT_DISABLED;

    // Calls given function 'f' for each source edge of this vertex. Also passes source index.
    inline void
    forEachSourceEdge(std::function<void(const DfgEdge&, size_t)> f) const VL_MT_DISABLED;

    // Calls given function 'f' for each sink vertex of this vertex
    // Unlinking/deleting the given sink during iteration is safe, but not other sinks of this
    // vertex.
    inline void forEachSink(std::function<void(DfgVertex&)> f) VL_MT_DISABLED;

    // Calls given function 'f' for each sink vertex of this vertex
    inline void forEachSink(std::function<void(const DfgVertex&)> f) const VL_MT_DISABLED;

    // Calls given function 'f' for each sink edge of this vertex.
    // Unlinking/deleting the given sink during iteration is safe, but not other sinks of this
    // vertex.
    inline void forEachSinkEdge(std::function<void(DfgEdge&)> f) VL_MT_DISABLED;

    // Calls given function 'f' for each sink edge of this vertex.
    inline void forEachSinkEdge(std::function<void(const DfgEdge&)> f) const VL_MT_DISABLED;

    // Returns first source edge which satisfies the given predicate 'p', or nullptr if no such
    // sink vertex exists
    inline const DfgEdge*
    findSourceEdge(std::function<bool(const DfgEdge&, size_t)> p) const VL_MT_DISABLED;

    // Returns first sink vertex of type 'Vertex' which satisfies the given predicate 'p',
    // or nullptr if no such sink vertex exists
    template <typename Vertex>
    inline Vertex* findSink(std::function<bool(const Vertex&)> p) const VL_MT_DISABLED;

    // Returns first sink vertex of type 'Vertex', or nullptr if no such sink vertex exists.
    // This is a special case of 'findSink' above with the predicate always true.
    template <typename Vertex>
    inline Vertex* findSink() const VL_MT_DISABLED;

    // Is this a DfgConst that is all zeroes
    inline bool isZero() const VL_MT_DISABLED;

    // Is this a DfgConst that is all ones
    inline bool isOnes() const VL_MT_DISABLED;

    // Should this vertex be inlined when rendering to Ast, or be stored to a temporary
    inline bool inlined() const VL_MT_DISABLED;

    // Methods that allow DfgVertex to participate in error reporting/messaging
    void v3errorEnd(std::ostringstream& str) const VL_RELEASE(V3Error::s().m_mutex) {
        m_filelinep->v3errorEnd(str);
    }
    void v3errorEndFatal(std::ostringstream& str) const VL_ATTR_NORETURN
        VL_RELEASE(V3Error::s().m_mutex) {
        m_filelinep->v3errorEndFatal(str);
    }
    string warnContextPrimary() const VL_REQUIRES(V3Error::s().m_mutex) {
        return fileline()->warnContextPrimary();
    }
    string warnContextSecondary() const { return fileline()->warnContextSecondary(); }
    string warnMore() const VL_REQUIRES(V3Error::s().m_mutex) { return fileline()->warnMore(); }
    string warnOther() const VL_REQUIRES(V3Error::s().m_mutex) { return fileline()->warnOther(); }

private:
    // For internal use only.
    // Note: specializations for particular vertex types are provided by 'astgen'
    template <typename T>
    inline static bool privateTypeTest(const DfgVertex* nodep);

public:
    // Subtype test
    template <typename T>
    bool is() const {
        static_assert(std::is_base_of<DfgVertex, T>::value, "'T' must be a subtype of DfgVertex");
        return privateTypeTest<typename std::remove_cv<T>::type>(this);
    }

    // Ensure subtype, then cast to that type
    template <typename T>
    T* as() {
        UASSERT_OBJ(is<T>(), this,
                    "DfgVertex is not of expected type, but instead has type '" << typeName()
                                                                                << "'");
        return static_cast<T*>(this);
    }
    template <typename T>
    const T* as() const {
        UASSERT_OBJ(is<T>(), this,
                    "DfgVertex is not of expected type, but instead has type '" << typeName()
                                                                                << "'");
        return static_cast<const T*>(this);
    }

    // Cast to subtype, or null if different
    template <typename T>
    T* cast() {
        return is<T>() ? static_cast<T*>(this) : nullptr;
    }
    template <typename T>
    const T* cast() const {
        return is<T>() ? static_cast<const T*>(this) : nullptr;
    }

    // Human-readable vertex type as string for debugging
    const string typeName() const { return m_type.ascii(); }

    // Human-readable name for source operand with given index for debugging
    virtual const string srcName(size_t idx) const = 0;
};

// Specializations of privateTypeTest
#include "V3Dfg__gen_type_tests.h"  // From ./astgen

//------------------------------------------------------------------------------
// Dfg vertex visitor
//------------------------------------------------------------------------------

class DfgVisitor VL_NOT_FINAL {
public:
    // Dispatch to most specific 'visit' method on 'vtxp'
    void iterate(DfgVertex* vtxp) { vtxp->accept(*this); }

    virtual void visit(DfgVertex* nodep) = 0;
#include "V3Dfg__gen_visitor_decls.h"  // From ./astgen
};

//------------------------------------------------------------------------------
// Inline method definitions
//------------------------------------------------------------------------------

void DfgGraph::addVertex(DfgVertex& vtx) {
    // Note: changes here need to be replicated in DfgGraph::addGraph
    ++m_size;
    if (vtx.is<DfgConst>()) {
        vtx.m_verticesEnt.pushBack(m_constVertices, &vtx);
    } else if (vtx.is<DfgVertexVar>()) {
        vtx.m_verticesEnt.pushBack(m_varVertices, &vtx);
    } else {
        vtx.m_verticesEnt.pushBack(m_opVertices, &vtx);
    }
    vtx.m_userCnt = 0;
    vtx.m_graphp = this;
}

void DfgGraph::removeVertex(DfgVertex& vtx) {
    // Note: changes here need to be replicated in DfgGraph::addGraph
    --m_size;
    if (vtx.is<DfgConst>()) {
        vtx.m_verticesEnt.unlink(m_constVertices, &vtx);
    } else if (vtx.is<DfgVertexVar>()) {
        vtx.m_verticesEnt.unlink(m_varVertices, &vtx);
    } else {
        vtx.m_verticesEnt.unlink(m_opVertices, &vtx);
    }
    vtx.m_userCnt = 0;
    vtx.m_graphp = nullptr;
}

void DfgGraph::forEachVertex(std::function<void(DfgVertex&)> f) {
    for (DfgVertex *vtxp = m_varVertices.begin(), *nextp; vtxp; vtxp = nextp) {
        nextp = vtxp->verticesNext();
        f(*vtxp);
    }
    for (DfgVertex *vtxp = m_constVertices.begin(), *nextp; vtxp; vtxp = nextp) {
        nextp = vtxp->verticesNext();
        f(*vtxp);
    }
    for (DfgVertex *vtxp = m_opVertices.begin(), *nextp; vtxp; vtxp = nextp) {
        nextp = vtxp->verticesNext();
        f(*vtxp);
    }
}

void DfgGraph::forEachVertex(std::function<void(const DfgVertex&)> f) const {
    for (const DfgVertex* vtxp = m_varVertices.begin(); vtxp; vtxp = vtxp->verticesNext()) {
        f(*vtxp);
    }
    for (const DfgVertex* vtxp = m_constVertices.begin(); vtxp; vtxp = vtxp->verticesNext()) {
        f(*vtxp);
    }
    for (const DfgVertex* vtxp = m_opVertices.begin(); vtxp; vtxp = vtxp->verticesNext()) {
        f(*vtxp);
    }
}

void DfgVertex::forEachSource(std::function<void(DfgVertex&)> f) {
    const auto pair = sourceEdges();
    const DfgEdge* const edgesp = pair.first;
    const size_t arity = pair.second;
    for (size_t i = 0; i < arity; ++i) {
        if (DfgVertex* const sourcep = edgesp[i].m_sourcep) f(*sourcep);
    }
}

void DfgVertex::forEachSource(std::function<void(const DfgVertex&)> f) const {
    const auto pair = sourceEdges();
    const DfgEdge* const edgesp = pair.first;
    const size_t arity = pair.second;
    for (size_t i = 0; i < arity; ++i) {
        if (DfgVertex* const sourcep = edgesp[i].m_sourcep) f(*sourcep);
    }
}

void DfgVertex::forEachSink(std::function<void(DfgVertex&)> f) {
    for (const DfgEdge *edgep = m_sinksp, *nextp; edgep; edgep = nextp) {
        nextp = edgep->m_nextp;
        f(*edgep->m_sinkp);
    }
}

void DfgVertex::forEachSink(std::function<void(const DfgVertex&)> f) const {
    for (const DfgEdge* edgep = m_sinksp; edgep; edgep = edgep->m_nextp) f(*edgep->m_sinkp);
}

void DfgVertex::forEachSourceEdge(std::function<void(DfgEdge&, size_t)> f) {
    const auto pair = sourceEdges();
    DfgEdge* const edgesp = pair.first;
    const size_t arity = pair.second;
    for (size_t i = 0; i < arity; ++i) f(edgesp[i], i);
}

void DfgVertex::forEachSourceEdge(std::function<void(const DfgEdge&, size_t)> f) const {
    const auto pair = sourceEdges();
    const DfgEdge* const edgesp = pair.first;
    const size_t arity = pair.second;
    for (size_t i = 0; i < arity; ++i) f(edgesp[i], i);
}

void DfgVertex::forEachSinkEdge(std::function<void(DfgEdge&)> f) {
    for (DfgEdge *edgep = m_sinksp, *nextp; edgep; edgep = nextp) {
        nextp = edgep->m_nextp;
        f(*edgep);
    }
}

void DfgVertex::forEachSinkEdge(std::function<void(const DfgEdge&)> f) const {
    for (DfgEdge *edgep = m_sinksp, *nextp; edgep; edgep = nextp) {
        nextp = edgep->m_nextp;
        f(*edgep);
    }
}

const DfgEdge* DfgVertex::findSourceEdge(std::function<bool(const DfgEdge&, size_t)> p) const {
    const auto pair = sourceEdges();
    const DfgEdge* const edgesp = pair.first;
    const size_t arity = pair.second;
    for (size_t i = 0; i < arity; ++i) {
        const DfgEdge& edge = edgesp[i];
        if (p(edge, i)) return &edge;
    }
    return nullptr;
}

template <typename Vertex>
Vertex* DfgVertex::findSink(std::function<bool(const Vertex&)> p) const {
    static_assert(std::is_base_of<DfgVertex, Vertex>::value,
                  "'Vertex' must be subclass of 'DfgVertex'");
    for (DfgEdge* edgep = m_sinksp; edgep; edgep = edgep->m_nextp) {
        if (Vertex* const sinkp = edgep->m_sinkp->cast<Vertex>()) {
            if (p(*sinkp)) return sinkp;
        }
    }
    return nullptr;
}

template <typename Vertex>
Vertex* DfgVertex::findSink() const {
    static_assert(!std::is_same<DfgVertex, Vertex>::value,
                  "'Vertex' must be proper subclass of 'DfgVertex'");
    return findSink<Vertex>([](const Vertex&) { return true; });
}

//------------------------------------------------------------------------------
// DfgVertex sub-types follow
//------------------------------------------------------------------------------

// Include macros generated by 'astgen'. These include DFGGEN_MEMBERS_<Node>
// for each DfgVertex sub-type. The generated members include boilerplate
// methods related to cloning, visitor dispatch, and other functionality.
// For precise details please read the generated macros.
#include "V3Dfg__gen_macros.h"

//------------------------------------------------------------------------------
// Implementation of dataflow graph vertices with a fixed number of sources
//------------------------------------------------------------------------------

template <size_t Arity>
class DfgVertexWithArity VL_NOT_FINAL : public DfgVertex {
    static_assert(1 <= Arity && Arity <= 4, "Arity must be between 1 and 4 inclusive");

    std::array<DfgEdge, Arity> m_srcs;  // Source edges

protected:
    DfgVertexWithArity(DfgGraph& dfg, VDfgType type, FileLine* flp, AstNodeDType* dtypep)
        : DfgVertex{dfg, type, flp, dtypep} {
        // Initialize source edges
        for (size_t i = 0; i < Arity; ++i) m_srcs[i].init(this);
    }

    ~DfgVertexWithArity() override = default;

public:
    std::pair<DfgEdge*, size_t> sourceEdges() final override {  //
        return {m_srcs.data(), Arity};
    }
    std::pair<const DfgEdge*, size_t> sourceEdges() const final override {
        return {m_srcs.data(), Arity};
    }

    template <size_t Index>
    DfgEdge* sourceEdge() {
        static_assert(Index < Arity, "Source index out of range");
        return &m_srcs[Index];
    }

    template <size_t Index>
    const DfgEdge* sourceEdge() const {
        static_assert(Index < Arity, "Source index out of range");
        return &m_srcs[Index];
    }

    template <size_t Index>
    DfgVertex* source() const {
        static_assert(Index < Arity, "Source index out of range");
        return m_srcs[Index].sourcep();
    }

    template <size_t Index>
    void relinkSource(DfgVertex* newSourcep) {
        static_assert(Index < Arity, "Source index out of range");
        UASSERT_OBJ(m_srcs[Index].sinkp() == this, this, "Inconsistent");
        m_srcs[Index].relinkSource(newSourcep);
    }
};

class DfgVertexUnary VL_NOT_FINAL : public DfgVertexWithArity<1> {
protected:
    DfgVertexUnary(DfgGraph& dfg, VDfgType type, FileLine* flp, AstNodeDType* dtypep)
        : DfgVertexWithArity<1>{dfg, type, flp, dtypep} {}

public:
    ASTGEN_MEMBERS_DfgVertexUnary;

    // Named getter/setter for sources
    DfgVertex* srcp() const { return source<0>(); }
    void srcp(DfgVertex* vtxp) { relinkSource<0>(vtxp); }
};

class DfgVertexBinary VL_NOT_FINAL : public DfgVertexWithArity<2> {
protected:
    DfgVertexBinary(DfgGraph& dfg, VDfgType type, FileLine* flp, AstNodeDType* dtypep)
        : DfgVertexWithArity<2>{dfg, type, flp, dtypep} {}

public:
    ASTGEN_MEMBERS_DfgVertexBinary;

    // Named getter/setter for sources
    DfgVertex* lhsp() const { return source<0>(); }
    void lhsp(DfgVertex* vtxp) { relinkSource<0>(vtxp); }
    DfgVertex* rhsp() const { return source<1>(); }
    void rhsp(DfgVertex* vtxp) { relinkSource<1>(vtxp); }
};

class DfgVertexTernary VL_NOT_FINAL : public DfgVertexWithArity<3> {
protected:
    DfgVertexTernary(DfgGraph& dfg, VDfgType type, FileLine* flp, AstNodeDType* dtypep)
        : DfgVertexWithArity<3>{dfg, type, flp, dtypep} {}

public:
    ASTGEN_MEMBERS_DfgVertexTernary;
};

//------------------------------------------------------------------------------
// Implementation of dataflow graph vertices with a variable number of sources
//------------------------------------------------------------------------------

class DfgVertexVariadic VL_NOT_FINAL : public DfgVertex {
    DfgEdge* m_srcsp;  // The source edges
    uint32_t m_srcCnt = 0;  // Number of sources used
    uint32_t m_srcCap;  // Number of sources allocated

    // Allocate a new source edge array
    DfgEdge* allocSources(size_t n) {
        DfgEdge* const srcsp = new DfgEdge[n];
        for (size_t i = 0; i < n; ++i) srcsp[i].init(this);
        return srcsp;
    }

    // Double the capacity of m_srcsp
    void growSources() {
        m_srcCap *= 2;
        DfgEdge* const newsp = allocSources(m_srcCap);
        for (size_t i = 0; i < m_srcCnt; ++i) {
            DfgEdge* const oldp = m_srcsp + i;
            // Skip over unlinked source edge
            if (!oldp->sourcep()) continue;
            // New edge driven from the same vertex as the old edge
            newsp[i].relinkSource(oldp->sourcep());
            // Unlink the old edge, it will be deleted
            oldp->unlinkSource();
        }
        // Delete old source edges
        delete[] m_srcsp;
        // Keep hold of new source edges
        m_srcsp = newsp;
    }

protected:
    DfgVertexVariadic(DfgGraph& dfg, VDfgType type, FileLine* flp, AstNodeDType* dtypep,
                      uint32_t initialCapacity = 1)
        : DfgVertex{dfg, type, flp, dtypep}
        , m_srcsp{allocSources(initialCapacity)}
        , m_srcCap{initialCapacity} {}

    ~DfgVertexVariadic() override { delete[] m_srcsp; };

    DfgEdge* addSource() {
        if (m_srcCnt == m_srcCap) growSources();
        return m_srcsp + m_srcCnt++;
    }

    void resetSources() {
        // #ifdef VL_DEBUG TODO: DEBUG ONLY
        for (uint32_t i = 0; i < m_srcCnt; ++i) {
            UASSERT_OBJ(!m_srcsp[i].sourcep(), m_srcsp[i].sourcep(), "Connected source");
        }
        // #endif
        m_srcCnt = 0;
    }

public:
    ASTGEN_MEMBERS_DfgVertexVariadic;

    DfgEdge* sourceEdge(size_t idx) const { return &m_srcsp[idx]; }
    DfgVertex* source(size_t idx) const { return m_srcsp[idx].sourcep(); }

    std::pair<DfgEdge*, size_t> sourceEdges() override { return {m_srcsp, m_srcCnt}; }
    std::pair<const DfgEdge*, size_t> sourceEdges() const override { return {m_srcsp, m_srcCnt}; }
};

// DfgVertex subclasses
#include "V3DfgVertices.h"

// The rest of the DfgVertex subclasses are generated by 'astgen' from AstNodeExpr nodes
#include "V3Dfg__gen_auto_classes.h"

DfgVertexVar* DfgGraph::varVerticesBeginp() const {
    return static_cast<DfgVertexVar*>(m_varVertices.begin());
}
DfgVertexVar* DfgGraph::varVerticesRbeginp() const {
    return static_cast<DfgVertexVar*>(m_varVertices.rbegin());
}
DfgConst* DfgGraph::constVerticesBeginp() const {
    return static_cast<DfgConst*>(m_constVertices.begin());
}
DfgConst* DfgGraph::constVerticesRbeginp() const {
    return static_cast<DfgConst*>(m_constVertices.rbegin());
}
DfgVertex* DfgGraph::opVerticesBeginp() const { return m_opVertices.begin(); }
DfgVertex* DfgGraph::opVerticesRbeginp() const { return m_opVertices.rbegin(); }

bool DfgVertex::isZero() const {
    if (const DfgConst* const constp = cast<DfgConst>()) return constp->isZero();
    return false;
}

bool DfgVertex::isOnes() const {
    if (const DfgConst* const constp = cast<DfgConst>()) return constp->isOnes();
    return false;
}

bool DfgVertex::inlined() const {
    // Inline vertices that drive only a single node, or are special
    if (!hasMultipleSinks()) return true;
    if (is<DfgConst>()) return true;
    if (is<DfgVertexVar>()) return true;
    if (const DfgArraySel* const selp = cast<DfgArraySel>()) return selp->bitp()->is<DfgConst>();
    return false;
}

#endif
