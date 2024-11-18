//===- VTableBuilder.h - C++ vtable layout builder --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code dealing with generation of the layout of virtual tables.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_VTABLEBUILDER_H
#define LLVM_CLANG_AST_VTABLEBUILDER_H

#include "clang/AST/BaseSubobject.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/ABI.h"
#include "clang/Basic/Thunk.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <utility>

namespace clang {
class CXXRecordDecl;
class ASTContext;
class FunctionDecl;
class CXXMethodDecl;
class CXXDestructorDecl;
class TemplateParameterList;

/// Represents a single component in a vtable.
class VTableComponent {
public:
  enum Kind {
    CK_VCallOffset,
    CK_VBaseOffset,
    CK_OffsetToTop,
    CK_RTTI,
    CK_FunctionPointer,

    /// A pointer to the complete destructor.
    CK_CompleteDtorPointer,

    /// A pointer to the deleting destructor.
    CK_DeletingDtorPointer,

    /// An entry that is never used.
    ///
    /// In some cases, a vtable function pointer will end up never being
    /// called. Such vtable function pointers are represented as a
    /// CK_UnusedFunctionPointer.
    CK_UnusedFunctionPointer,

    /// A static function pointer that can be called directly.
    CK_StaticFunctionPointer,

    /// A static lambda that can be called directly.
    CK_StaticLambda,

    /// A static std::function object's address.
    CK_StaticStdFunction,

    /// A virtual method with template parameters.
    CK_VirtualTemplateMethod,

    /// A virtual method that is inherited through virtual inheritance.
    CK_VirtualInheritedMethod,

    /// Template parameter information used during compilation.
    ///
    /// This component stores information about template parameters
    /// that is only needed during compilation and doesn't require
    /// runtime representation.
    CK_TemplateParamInfo
  };

private:
  Kind TheKind;
  union {
    CharUnits Offset;
    const void *Ptr;
    const TemplateParameterList *TemplateParams;
    struct {
      const void *FuncPtr;           // Function pointer or address
      const void *TemplateInfo;      // Template parameters if any
      const void *InheritanceInfo;   // Base class info for inheritance
      const void *VirtualInfo;       // Virtual inheritance info
    } StaticFunc;
  };

  bool IsMutable;
  bool HasTemplateParams;
  bool IsVirtualInherited;
  bool IsVirtualTemplate;

public:
  VTableComponent()
    : TheKind(CK_FunctionPointer),
      Ptr(nullptr),
      IsMutable(false),
      HasTemplateParams(false),
      IsVirtualInherited(false),
      IsVirtualTemplate(false) {}

  static VTableComponent MakeVCallOffset(CharUnits Offset) {
    VTableComponent Result;
    Result.TheKind = CK_VCallOffset;
    Result.Offset = Offset;
    return Result;
  }

  static VTableComponent MakeVBaseOffset(CharUnits Offset) {
    VTableComponent Result;
    Result.TheKind = CK_VBaseOffset;
    Result.Offset = Offset;
    return Result;
  }

  static VTableComponent MakeOffsetToTop(CharUnits Offset) {
    VTableComponent Result;
    Result.TheKind = CK_OffsetToTop;
    Result.Offset = Offset;
    return Result;
  }

  static VTableComponent MakeRTTI(const CXXRecordDecl *RD) {
    VTableComponent Result;
    Result.TheKind = CK_RTTI;
    Result.Ptr = RD;
    return Result;
  }

  static VTableComponent MakeFunction(const CXXMethodDecl *MD) {
    VTableComponent Result;
    Result.TheKind = CK_FunctionPointer;
    Result.Ptr = MD;
    return Result;
  }

  static VTableComponent MakeCompleteDtor(const CXXDestructorDecl *DD) {
    VTableComponent Result;
    Result.TheKind = CK_CompleteDtorPointer;
    Result.Ptr = DD;
    return Result;
  }

  static VTableComponent MakeDeletingDtor(const CXXDestructorDecl *DD) {
    VTableComponent Result;
    Result.TheKind = CK_DeletingDtorPointer;
    Result.Ptr = DD;
    return Result;
  }

  static VTableComponent MakeUnusedFunction(const CXXMethodDecl *MD) {
    VTableComponent Result;
    Result.TheKind = CK_UnusedFunctionPointer;
    Result.Ptr = MD;
    return Result;
  }

  static VTableComponent MakeStaticFunction(const FunctionDecl *FD,
                                         ASTContext &Context,
                                         const void *Addr = nullptr,
                                         const CXXRecordDecl *InheritedFrom = nullptr,
                                         bool IsVirtual = false) {
    VTableComponent Result;
    Result.TheKind = CK_StaticFunctionPointer;
    Result.StaticFunc.FuncPtr = Addr ? Addr : FD;
    Result.StaticFunc.TemplateInfo = nullptr;
    Result.StaticFunc.InheritanceInfo = InheritedFrom;
    Result.IsVirtualInherited = IsVirtual;
    Result.HasTemplateParams = false;
    Result.IsMutable = false;
    return Result;
  }

  static VTableComponent MakeStaticLambda(const CXXMethodDecl *CallOp,
                                         ASTContext &Context,
                                         bool IsMutable = false,
                                         const void *Addr = nullptr,
                                         const CXXRecordDecl *InheritedFrom = nullptr,
                                         bool IsVirtual = false) {
    VTableComponent Result;
    Result.TheKind = CK_StaticLambda;
    Result.StaticFunc.FuncPtr = Addr ? Addr : CallOp;
    Result.StaticFunc.TemplateInfo = nullptr;
    Result.StaticFunc.InheritanceInfo = InheritedFrom;
    Result.IsVirtualInherited = IsVirtual;
    Result.HasTemplateParams = false;
    Result.IsMutable = IsMutable;
    return Result;
  }

  static VTableComponent MakeStaticStdFunction(const FunctionDecl *FD,
                                              ASTContext &Context,
                                              const TemplateParameterList *TPL = nullptr,
                                              const void *Addr = nullptr,
                                              const CXXRecordDecl *InheritedFrom = nullptr) {
    VTableComponent Result;
    Result.TheKind = CK_StaticStdFunction;
    Result.StaticFunc.FuncPtr = Addr ? Addr : FD;
    Result.StaticFunc.TemplateInfo = TPL;
    Result.StaticFunc.InheritanceInfo = InheritedFrom;
    Result.StaticFunc.VirtualInfo = nullptr;
    Result.IsVirtualInherited = false;
    Result.HasTemplateParams = TPL != nullptr;
    Result.IsMutable = false;
    Result.IsVirtualTemplate = false;
    return Result;
  }

  static VTableComponent MakeVirtualTemplateMethod(const CXXMethodDecl *MD,
                                                   const TemplateParameterList *TPL,
                                                   const void *Addr = nullptr,
                                                   const CXXRecordDecl *InheritedFrom = nullptr) {
    VTableComponent Result;
    Result.TheKind = CK_VirtualTemplateMethod;
    Result.StaticFunc.FuncPtr = Addr ? Addr : MD;
    Result.StaticFunc.TemplateInfo = TPL;
    Result.StaticFunc.InheritanceInfo = InheritedFrom;
    Result.StaticFunc.VirtualInfo = nullptr;
    Result.IsVirtualInherited = false;
    Result.HasTemplateParams = true;
    Result.IsMutable = false;
    Result.IsVirtualTemplate = true;
    return Result;
  }

  static VTableComponent MakeVirtualInheritedMethod(const CXXMethodDecl *MD,
                                                    const CXXRecordDecl *VBase,
                                                    const void *Addr = nullptr,
                                                    const CXXRecordDecl *InheritedFrom = nullptr) {
    VTableComponent Result;
    Result.TheKind = CK_VirtualInheritedMethod;
    Result.StaticFunc.FuncPtr = Addr ? Addr : MD;
    Result.StaticFunc.TemplateInfo = nullptr;
    Result.StaticFunc.InheritanceInfo = InheritedFrom;
    Result.StaticFunc.VirtualInfo = VBase;
    Result.IsVirtualInherited = true;
    Result.HasTemplateParams = false;
    Result.IsMutable = false;
    Result.IsVirtualTemplate = false;
    return Result;
  }

  static VTableComponent MakeTemplateParamInfo(const TemplateParameterList *TPL,
                                              const CXXRecordDecl *InheritedFrom = nullptr) {
    VTableComponent Result;
    Result.TheKind = CK_TemplateParamInfo;
    Result.TemplateParams = TPL;
    Result.StaticFunc.InheritanceInfo = InheritedFrom;
    Result.HasTemplateParams = true;
    return Result;
  }

  Kind getKind() const { return TheKind; }

  CharUnits getVCallOffset() const {
    assert(TheKind == CK_VCallOffset && "Invalid component kind!");
    return Offset;
  }

  CharUnits getVBaseOffset() const {
    assert(TheKind == CK_VBaseOffset && "Invalid component kind!");
    return Offset;
  }

  CharUnits getOffsetToTop() const {
    assert(TheKind == CK_OffsetToTop && "Invalid component kind!");
    return Offset;
  }

  const CXXRecordDecl *getRTTIDecl() const {
    assert(TheKind == CK_RTTI && "Invalid component kind!");
    return static_cast<const CXXRecordDecl *>(Ptr);
  }

  const CXXMethodDecl *getFunctionDecl() const {
    assert(TheKind == CK_FunctionPointer && "Invalid component kind!");
    return static_cast<const CXXMethodDecl *>(Ptr);
  }

  const FunctionDecl *getStaticFunctionDecl() const {
    assert(TheKind == CK_StaticFunctionPointer && "Invalid component kind!");
    return static_cast<const FunctionDecl *>(StaticFunc.FuncPtr);
  }

  const CXXMethodDecl *getStaticLambdaDecl() const {
    assert(TheKind == CK_StaticLambda && "Invalid component kind!");
    return static_cast<const CXXMethodDecl *>(StaticFunc.FuncPtr);
  }

  const FunctionDecl *getStaticStdFunctionDecl() const {
    assert(TheKind == CK_StaticStdFunction && "Invalid component kind!");
    return static_cast<const FunctionDecl *>(StaticFunc.FuncPtr);
  }

  const CXXMethodDecl *getVirtualTemplateMethodDecl() const {
    assert(TheKind == CK_VirtualTemplateMethod && "Invalid component kind!");
    return static_cast<const CXXMethodDecl *>(StaticFunc.FuncPtr);
  }

  const CXXMethodDecl *getVirtualInheritedMethodDecl() const {
    assert(TheKind == CK_VirtualInheritedMethod && "Invalid component kind!");
    return static_cast<const CXXMethodDecl *>(StaticFunc.FuncPtr);
  }

  const CXXRecordDecl *getVirtualBaseClass() const {
    assert(TheKind == CK_VirtualInheritedMethod && "Invalid component kind!");
    return static_cast<const CXXRecordDecl *>(StaticFunc.VirtualInfo);
  }

  const TemplateParameterList *getTemplateParams() const {
    assert((TheKind == CK_TemplateParamInfo || TheKind == CK_VirtualTemplateMethod) &&
           "Invalid component kind!");
    return TheKind == CK_TemplateParamInfo ? TemplateParams :
           static_cast<const TemplateParameterList *>(StaticFunc.TemplateInfo);
  }

  bool isStaticKind() const {
    return TheKind == CK_StaticFunctionPointer ||
           TheKind == CK_StaticLambda ||
           TheKind == CK_StaticStdFunction;
  }

  bool isVirtualKind() const {
    return TheKind == CK_VirtualTemplateMethod ||
           TheKind == CK_VirtualInheritedMethod;
  }

  bool isMutableLambda() const {
    return TheKind == CK_StaticLambda && IsMutable;
  }

  bool isVirtualTemplateMethod() const {
    return TheKind == CK_VirtualTemplateMethod;
  }

  bool isVirtualInheritedMethod() const {
    return TheKind == CK_VirtualInheritedMethod;
  }

  const void *getStaticFuncPtr() const {
    assert((isStaticKind() || isVirtualKind()) && "Invalid component kind!");
    return StaticFunc.FuncPtr;
  }

  bool isUsedFunctionPointer() const {
    return isUsedFunctionPointerKind(getKind());
  }

  static bool isFunctionPointerKind(Kind ComponentKind) {
    return ComponentKind == CK_FunctionPointer ||
           ComponentKind == CK_StaticFunctionPointer ||
           ComponentKind == CK_StaticStdFunction ||
           ComponentKind == CK_StaticLambda ||
           ComponentKind == CK_CompleteDtorPointer ||
           ComponentKind == CK_DeletingDtorPointer;
  }

  static bool isUsedFunctionPointerKind(Kind ComponentKind) {
    return ComponentKind == CK_FunctionPointer ||
           ComponentKind == CK_StaticFunctionPointer ||
           ComponentKind == CK_StaticStdFunction ||
           ComponentKind == CK_StaticLambda ||
           ComponentKind == CK_CompleteDtorPointer ||
           ComponentKind == CK_DeletingDtorPointer;
  }

  bool operator==(const VTableComponent &Other) const {
    if (TheKind != Other.TheKind)
      return false;

    // Check template parameters
    if (TheKind == CK_TemplateParamInfo)
      return TemplateParams == Other.TemplateParams &&
             HasTemplateParams == Other.HasTemplateParams;

    // Check offsets
    if (TheKind == CK_VCallOffset || TheKind == CK_VBaseOffset ||
        TheKind == CK_OffsetToTop)
      return Offset == Other.Offset;

    // Check static function components
    if (isStaticKind()) {
      return StaticFunc.FuncPtr == Other.StaticFunc.FuncPtr &&
             StaticFunc.TemplateInfo == Other.StaticFunc.TemplateInfo &&
             StaticFunc.InheritanceInfo == Other.StaticFunc.InheritanceInfo &&
             IsMutable == Other.IsMutable &&
             HasTemplateParams == Other.HasTemplateParams &&
             IsVirtualInherited == Other.IsVirtualInherited;
    }

    // Default pointer comparison
    return Ptr == Other.Ptr;
  }

  bool operator!=(const VTableComponent &Other) const {
    return !(*this == Other);
  }
};

class VTableLayout {
public:
  typedef std::pair<uint64_t, ThunkInfo> VTableThunkTy;
  struct AddressPointLocation {
    unsigned VTableIndex, AddressPointIndex;
  };
  typedef llvm::DenseMap<BaseSubobject, AddressPointLocation> AddressPointsMapTy;
  typedef llvm::SmallVector<unsigned, 4> AddressPointsIndexMapTy;

private:
  OwningArrayRef<size_t> VTableIndices;
  OwningArrayRef<VTableComponent> VTableComponents;
  OwningArrayRef<VTableThunkTy> VTableThunks;
  AddressPointsMapTy AddressPoints;
  AddressPointsIndexMapTy AddressPointsIndex;

public:
  VTableLayout(ArrayRef<size_t> VTableIndices,
               ArrayRef<VTableComponent> VTableComponents,
               ArrayRef<VTableThunkTy> VTableThunks,
               const AddressPointsMapTy &AddressPoints);
  ~VTableLayout();

  ArrayRef<VTableComponent> vtable_components() const {
    return VTableComponents;
  }

  ArrayRef<VTableThunkTy> vtable_thunks() const {
    return VTableThunks;
  }

  const AddressPointsMapTy &getAddressPoints() const {
    return AddressPoints;
  }

  const AddressPointsIndexMapTy &getAddressPointsIndex() const {
    return AddressPointsIndex;
  }

  size_t getNumVTables() const {
    return VTableIndices.empty() ? 1 : VTableIndices.size();
  }

  size_t getVTableOffset(size_t i) const {
    return VTableIndices.empty() ? 0 : VTableIndices[i];
  }

  size_t getVTableSize(size_t i) const {
    size_t thisIndex = getVTableOffset(i);
    size_t nextIndex = (i + 1 == getNumVTables())
                          ? VTableComponents.size()
                          : getVTableOffset(i + 1);
    return nextIndex - thisIndex;
  }
};

class VTableContextBase {
public:
  typedef SmallVector<ThunkInfo, 1> ThunkInfoVectorTy;

  bool isMicrosoft() const { return IsMicrosoftABI; }

  virtual ~VTableContextBase() {}

protected:
  typedef llvm::DenseMap<const CXXMethodDecl *, ThunkInfoVectorTy> ThunksMapTy;

  /// Contains all thunks that a given method decl will need.
  ThunksMapTy Thunks;

  /// Compute and store all vtable related information (vtable layout, vbase
  /// offset offsets, thunks etc) for the given record decl.
  virtual void computeVTableRelatedInformation(const CXXRecordDecl *RD) = 0;

  VTableContextBase(bool MS) : IsMicrosoftABI(MS) {}

public:
  virtual const ThunkInfoVectorTy *getThunkInfo(GlobalDecl GD);

  bool IsMicrosoftABI;

  /// Determine whether this function should be assigned a vtable slot.
  static bool hasVtableSlot(const CXXMethodDecl *MD);
};

class ItaniumVTableContext : public VTableContextBase {
public:
  typedef llvm::DenseMap<const CXXMethodDecl *, const CXXMethodDecl *>
      OriginalMethodMapTy;

private:

  /// Contains the index (relative to the vtable address point)
  /// where the function pointer for a virtual function is stored.
  typedef llvm::DenseMap<GlobalDecl, int64_t> MethodVTableIndicesTy;
  MethodVTableIndicesTy MethodVTableIndices;

  typedef llvm::DenseMap<const CXXRecordDecl *,
                         std::unique_ptr<const VTableLayout>>
      VTableLayoutMapTy;
  VTableLayoutMapTy VTableLayouts;

  typedef std::pair<const CXXRecordDecl *,
                    const CXXRecordDecl *> ClassPairTy;

  /// vtable offsets for offsets of virtual bases of a class.
  ///
  /// Contains the vtable offset (relative to the address point) in chars
  /// where the offsets for virtual bases of a class are stored.
  typedef llvm::DenseMap<ClassPairTy, CharUnits>
    VirtualBaseClassOffsetOffsetsMapTy;
  VirtualBaseClassOffsetOffsetsMapTy VirtualBaseClassOffsetOffsets;

  /// Map from a virtual method to the nearest method in the primary base class
  /// chain that it overrides.
  OriginalMethodMapTy OriginalMethodMap;

  void computeVTableRelatedInformation(const CXXRecordDecl *RD) override;

public:
  enum VTableComponentLayout {
    /// Components in the vtable are pointers to other structs/functions.
    Pointer,

    /// Components in the vtable are relative offsets between the vtable and the
    /// other structs/functions.
    Relative,
  };

  ItaniumVTableContext(ASTContext &Context,
                       VTableComponentLayout ComponentLayout = Pointer);
  ~ItaniumVTableContext() override;

  const VTableLayout &getVTableLayout(const CXXRecordDecl *RD) {
    computeVTableRelatedInformation(RD);
    assert(VTableLayouts.count(RD) && "No layout for this record decl!");

    return *VTableLayouts[RD];
  }

  std::unique_ptr<VTableLayout> createConstructionVTableLayout(
      const CXXRecordDecl *MostDerivedClass, CharUnits MostDerivedClassOffset,
      bool MostDerivedClassIsVirtual, const CXXRecordDecl *LayoutClass);

  /// Locate a virtual function in the vtable.
  ///
  /// Return the index (relative to the vtable address point) where the
  /// function pointer for the given virtual function is stored.
  uint64_t getMethodVTableIndex(GlobalDecl GD);

  /// Return the offset in chars (relative to the vtable address point) where
  /// the offset of the virtual base that contains the given base is stored,
  /// otherwise, if no virtual base contains the given class, return 0.
  ///
  /// Base must be a virtual base class or an unambiguous base.
  CharUnits getVirtualBaseOffsetOffset(const CXXRecordDecl *RD,
                                       const CXXRecordDecl *VBase);

  /// Return the method that added the v-table slot that will be used to call
  /// the given method.
  ///
  /// In the Itanium ABI, where overrides always cause methods to be added to
  /// the primary v-table if they're not already there, this will be the first
  /// declaration in the primary base class chain for which the return type
  /// adjustment is trivial.
  GlobalDecl findOriginalMethod(GlobalDecl GD);

  const CXXMethodDecl *findOriginalMethodInMap(const CXXMethodDecl *MD) const;

  void setOriginalMethod(const CXXMethodDecl *Key, const CXXMethodDecl *Val) {
    OriginalMethodMap[Key] = Val;
  }

  /// This method is reserved for the implementation and shouldn't be used
  /// directly.
  const OriginalMethodMapTy &getOriginalMethodMap() {
    return OriginalMethodMap;
  }

  static bool classof(const VTableContextBase *VT) {
    return !VT->isMicrosoft();
  }

  VTableComponentLayout getVTableComponentLayout() const {
    return ComponentLayout;
  }

  bool isPointerLayout() const { return ComponentLayout == Pointer; }
  bool isRelativeLayout() const { return ComponentLayout == Relative; }

private:
  VTableComponentLayout ComponentLayout;
};

/// Holds information about the inheritance path to a virtual base or function
/// table pointer.  A record may contain as many vfptrs or vbptrs as there are
/// base subobjects.
struct VPtrInfo {
  typedef SmallVector<const CXXRecordDecl *, 1> BasePath;

  VPtrInfo(const CXXRecordDecl *RD)
      : ObjectWithVPtr(RD), IntroducingObject(RD), NextBaseToMangle(RD) {}

  /// This is the most derived class that has this vptr at offset zero. When
  /// single inheritance is used, this is always the most derived class. If
  /// multiple inheritance is used, it may be any direct or indirect base.
  const CXXRecordDecl *ObjectWithVPtr;

  /// This is the class that introduced the vptr by declaring new virtual
  /// methods or virtual bases.
  const CXXRecordDecl *IntroducingObject;

  /// IntroducingObject is at this offset from its containing complete object or
  /// virtual base.
  CharUnits NonVirtualOffset;

  /// The bases from the inheritance path that got used to mangle the vbtable
  /// name.  This is not really a full path like a CXXBasePath.  It holds the
  /// subset of records that need to be mangled into the vbtable symbol name in
  /// order to get a unique name.
  BasePath MangledPath;

  /// The next base to push onto the mangled path if this path is ambiguous in a
  /// derived class.  If it's null, then it's already been pushed onto the path.
  const CXXRecordDecl *NextBaseToMangle;

  /// The set of possibly indirect vbases that contain this vbtable.  When a
  /// derived class indirectly inherits from the same vbase twice, we only keep
  /// vtables and their paths from the first instance.
  BasePath ContainingVBases;

  /// This holds the base classes path from the complete type to the first base
  /// with the given vfptr offset, in the base-to-derived order.  Only used for
  /// vftables.
  BasePath PathToIntroducingObject;

  /// Static offset from the top of the most derived class to this vfptr,
  /// including any virtual base offset.  Only used for vftables.
  CharUnits FullOffsetInMDC;

  /// The vptr is stored inside the non-virtual component of this virtual base.
  const CXXRecordDecl *getVBaseWithVPtr() const {
    return ContainingVBases.empty() ? nullptr : ContainingVBases.front();
  }
};

typedef SmallVector<std::unique_ptr<VPtrInfo>, 2> VPtrInfoVector;

/// All virtual base related information about a given record decl.  Includes
/// information on all virtual base tables and the path components that are used
/// to mangle them.
struct VirtualBaseInfo {
  /// A map from virtual base to vbtable index for doing a conversion from the
  /// the derived class to the a base.
  llvm::DenseMap<const CXXRecordDecl *, unsigned> VBTableIndices;

  /// Information on all virtual base tables used when this record is the most
  /// derived class.
  VPtrInfoVector VBPtrPaths;
};

struct MethodVFTableLocation {
  /// If nonzero, holds the vbtable index of the virtual base with the vfptr.
  uint64_t VBTableIndex;

  /// If nonnull, holds the last vbase which contains the vfptr that the
  /// method definition is adjusted to.
  const CXXRecordDecl *VBase;

  /// This is the offset of the vfptr from the start of the last vbase, or the
  /// complete type if there are no virtual bases.
  CharUnits VFPtrOffset;

  /// Method's index in the vftable.
  uint64_t Index;

  MethodVFTableLocation()
      : VBTableIndex(0), VBase(nullptr), VFPtrOffset(CharUnits::Zero()),
        Index(0) {}

  MethodVFTableLocation(uint64_t VBTableIndex, const CXXRecordDecl *VBase,
                        CharUnits VFPtrOffset, uint64_t Index)
      : VBTableIndex(VBTableIndex), VBase(VBase), VFPtrOffset(VFPtrOffset),
        Index(Index) {}

  bool operator<(const MethodVFTableLocation &other) const {
    if (VBTableIndex != other.VBTableIndex) {
      assert(VBase != other.VBase);
      return VBTableIndex < other.VBTableIndex;
    }
    return std::tie(VFPtrOffset, Index) <
           std::tie(other.VFPtrOffset, other.Index);
  }
};

class MicrosoftVTableContext : public VTableContextBase {
public:
  MicrosoftVTableContext(ASTContext &Context)
      : VTableContextBase(/*MS=*/true), Context(Context) {}

  ~MicrosoftVTableContext() override;

  const VPtrInfoVector &getVFPtrOffsets(const CXXRecordDecl *RD);

  const VTableLayout &getVFTableLayout(const CXXRecordDecl *RD,
                                       CharUnits VFPtrOffset);

  MethodVFTableLocation getMethodVFTableLocation(GlobalDecl GD);

  const ThunkInfoVectorTy *getThunkInfo(GlobalDecl GD) override {
    // Complete destructors don't have a slot in a vftable, so no thunks needed.
    if (isa<CXXDestructorDecl>(GD.getDecl()) &&
        GD.getDtorType() == Dtor_Complete)
      return nullptr;
    return VTableContextBase::getThunkInfo(GD);
  }

  /// Returns the index of VBase in the vbtable of Derived.
  /// VBase must be a morally virtual base of Derived.
  /// The vbtable is an array of i32 offsets.  The first entry is a self entry,
  /// and the rest are offsets from the vbptr to virtual bases.
  unsigned getVBTableIndex(const CXXRecordDecl *Derived,
                           const CXXRecordDecl *VBase);

  const VPtrInfoVector &enumerateVBTables(const CXXRecordDecl *RD);

  static bool classof(const VTableContextBase *VT) { return VT->isMicrosoft(); }

  /// Get the address of a static function or lambda for VMT entry
  const void *getStaticFunctionAddress(const FunctionDecl *FD) const;

  /// Get the address of a std::function object for VMT entry
  const void *getStdFunctionAddress(const FunctionDecl *FD,
                                   const TemplateParameterList *TPL = nullptr) const;

  /// Handle template parameter preservation for virtual methods
  const TemplateParameterList *preserveTemplateParams(const CXXMethodDecl *MD,
                                                     const CXXRecordDecl *Inherited = nullptr) const;

  /// Support for virtual inheritance in VMT
  bool hasVirtualBase(const CXXRecordDecl *RD) const;

private:
  ASTContext &Context;

  typedef llvm::DenseMap<GlobalDecl, MethodVFTableLocation>
    MethodVFTableLocationsTy;
  MethodVFTableLocationsTy MethodVFTableLocations;

  typedef llvm::DenseMap<const CXXRecordDecl *, std::unique_ptr<VPtrInfoVector>>
      VFPtrLocationsMapTy;
  VFPtrLocationsMapTy VFPtrLocations;

  typedef std::pair<const CXXRecordDecl *, CharUnits> VFTableIdTy;
  typedef llvm::DenseMap<VFTableIdTy, std::unique_ptr<const VTableLayout>>
      VFTableLayoutMapTy;
  VFTableLayoutMapTy VFTableLayouts;

  llvm::DenseMap<const CXXRecordDecl *, std::unique_ptr<VirtualBaseInfo>>
      VBaseInfo;

  /// Maps for storing static function and template information
  llvm::DenseMap<const FunctionDecl *, const void *> StaticFuncAddresses;
  llvm::DenseMap<const CXXMethodDecl *, const TemplateParameterList *> TemplateParamMap;

  void computeVTableRelatedInformation(const CXXRecordDecl *RD) override;

  void dumpMethodLocations(const CXXRecordDecl *RD,
                           const MethodVFTableLocationsTy &NewMethods,
                           raw_ostream &);

  const VirtualBaseInfo &
  computeVBTableRelatedInformation(const CXXRecordDecl *RD);

  void computeVTablePaths(bool ForVBTables, const CXXRecordDecl *RD,
                          VPtrInfoVector &Paths);

  /// Helper for dumping the vtable layout
  void dumpMethodLocation(GlobalDecl GD,
                         const VTableLayout &VTLayout,
                         llvm::raw_ostream &OS) const;
};

} // namespace clang

#endif // LLVM_CLANG_AST_VTABLEBUILDER_H
