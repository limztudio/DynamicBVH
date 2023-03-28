#pragma once


#include "CoreMinimal.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


namespace __hidden_DynamicBVH{
    template<typename SizeType>
    static constexpr SizeType NodeNull = static_cast<SizeType>(-1);

    template<typename FloatType>
    static constexpr FloatType FatExtension = static_cast<FloatType>(0.1f);
    template<typename FloatType>
    static constexpr FloatType AABBMultiplier = static_cast<FloatType>(4);

    template<typename FloatType>
    static constexpr FloatType FloatMin = 0;
    template<>
    static constexpr float FloatMin<float> = FLT_MIN;
    template<>
    static constexpr float FloatMin<double> = DBL_MIN;
    template<typename FloatType>
    static constexpr FloatType FloatMax = 0;
    template<>
    static constexpr float FloatMax<float> = FLT_MAX;
    template<>
    static constexpr float FloatMax<double> = DBL_MAX;


    template<typename FloatType>
    struct VectorRegisterFinder{ typedef void Type; };
    template<>
    struct VectorRegisterFinder<float>{ typedef VectorRegister4Float Type; };
    template<>
    struct VectorRegisterFinder<double>{ typedef VectorRegister4Double Type; };


    template<typename FloatType>
    static constexpr typename VectorRegisterFinder<FloatType>::Type VectorTwo = MakeVectorRegisterConstant(static_cast<FloatType>(2), static_cast<FloatType>(2), static_cast<FloatType>(2), static_cast<FloatType>(2));
    template<typename FloatType>
    static constexpr typename VectorRegisterFinder<FloatType>::Type VectorKindaSmallNumber = MakeVectorRegisterConstant(static_cast<FloatType>(0), static_cast<FloatType>(0), static_cast<FloatType>(0), static_cast<FloatType>(0));
    template<>
    static constexpr VectorRegisterFinder<float>::Type VectorKindaSmallNumber<float> = MakeVectorRegisterFloatConstant(KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER);
    template<>
    static constexpr VectorRegisterFinder<double>::Type VectorKindaSmallNumber<double> = MakeVectorRegisterDoubleConstant(DOUBLE_KINDA_SMALL_NUMBER, DOUBLE_KINDA_SMALL_NUMBER, DOUBLE_KINDA_SMALL_NUMBER, DOUBLE_KINDA_SMALL_NUMBER);
    

    template<typename FloatType>
    struct MS_ALIGN(16) AABB2D{
        typedef UE::Math::TVector2<FloatType> Type;
        typedef typename VectorRegisterFinder<FloatType>::Type VectorType;
        
        
        UE::Math::TVector4<FloatType> LowerUpper;


        void Expand(FloatType Expand){
            LowerUpper.X -= Expand;
            LowerUpper.Y -= Expand;
            LowerUpper.Z += Expand;
            LowerUpper.W += Expand;

            ensure(LowerUpper.X <= LowerUpper.Z);
            ensure(LowerUpper.Y <= LowerUpper.W);
        }
        static AABB2D Expand(const AABB2D& Rhs, FloatType Expand){
            AABB2D Ret = Rhs;
            Ret.Expand(Expand);
            return Ret;
        }

        void ExpandBySign(const Type& D){
            if(D.X < 0)
                LowerUpper.X += D.X;
            else
                LowerUpper.Z += D.X;
            if(D.Y < 0)
                LowerUpper.Y += D.Y;
            else
                LowerUpper.W += D.Y;

            ensure(LowerUpper.X <= LowerUpper.Z);
            ensure(LowerUpper.Y <= LowerUpper.W);
        }
        static AABB2D ExpandBySign(const AABB2D& Rhs, const Type& D){
            AABB2D Ret = Rhs;
            Ret.ExpandBySign(D);
            return Ret;
        }

        void Move(const Type& V){
            LowerUpper.X += V.X;
            LowerUpper.Y += V.Y;
            LowerUpper.Z += V.X;
            LowerUpper.W += V.Y;

            ensure(LowerUpper.X <= LowerUpper.Z);
            ensure(LowerUpper.Y <= LowerUpper.W);
        }
        static AABB2D Move(const AABB2D& Rhs, const Type& V){
            AABB2D Ret = Rhs;
            Ret.Move(V);
            return Ret;
        }
        
        void Combine(const AABB2D& AABB){
            VectorType XMMLhsLower = VectorLoadAligned(&LowerUpper);
            VectorType XMMLhsUpper = VectorSwizzle(XMMLhsLower, 2, 3, 0, 1);
            VectorType XMMRhsLower = VectorLoadAligned(&AABB.LowerUpper);
            VectorType XMMRhsUpper = VectorSwizzle(XMMRhsLower, 2, 3, 0, 1);

            VectorType XMMLower = VectorMin(XMMLhsLower, XMMRhsLower);
            VectorType XMMUpper = VectorMax(XMMLhsUpper, XMMRhsUpper);

            ensure((VectorMaskBits(VectorCompareLE(XMMLower, XMMUpper)) & 0x03) == 0x03);

            VectorType XMMLowerUpper = VectorShuffle(XMMLower, XMMUpper, 0, 1, 0, 1);
            
            VectorStoreAligned(XMMLowerUpper, &LowerUpper);
        }
        void Combine(const AABB2D& Lhs, const AABB2D& Rhs){
            VectorType XMMLhsLower = VectorLoadAligned(&Lhs.LowerUpper);
            VectorType XMMLhsUpper = VectorSwizzle(XMMLhsLower, 2, 3, 0, 1);
            VectorType XMMRhsLower = VectorLoadAligned(&Rhs.LowerUpper);
            VectorType XMMRhsUpper = VectorSwizzle(XMMRhsLower, 2, 3, 0, 1);

            VectorType XMMLower = VectorMin(XMMLhsLower, XMMRhsLower);
            VectorType XMMUpper = VectorMax(XMMLhsUpper, XMMRhsUpper);

            ensure((VectorMaskBits(VectorCompareLE(XMMLower, XMMUpper)) & 0x03) == 0x03);
            
            VectorType XMMLowerUpper = VectorShuffle(XMMLower, XMMUpper, 0, 1, 0, 1);

            VectorStoreAligned(XMMLowerUpper, &LowerUpper);
        }

        bool Contains(const AABB2D& AABB)const{
            VectorType XMMLhsLower = VectorLoadAligned(&LowerUpper);
            VectorType XMMRhsLower = VectorLoadAligned(&AABB.LowerUpper);
            if((VectorMaskBits(VectorCompareGT(XMMRhsLower, XMMLhsLower)) & 0x03) != 0)
                return false;

            VectorType XMMLhsUpper = VectorSwizzle(XMMLhsLower, 2, 3, 0, 1);
            VectorType XMMRhsUpper = VectorSwizzle(XMMRhsLower, 2, 3, 0, 1);
            if((VectorMaskBits(VectorCompareGT(XMMLhsUpper, XMMRhsUpper)) & 0x03) != 0)
                return false;

            return true;
        }
        bool Contains(const UE::Math::TVector4<FloatType>& Point)const{
            VectorType XMMRhs = VectorLoadAligned(&Point);

            VectorType XMMLhsLower = VectorLoadAligned(&LowerUpper);
            if((VectorMaskBits(VectorCompareGT(XMMRhs, XMMLhsLower)) & 0x03) != 0x03)
                return false;

            VectorType XMMLhsUpper = VectorSwizzle(XMMLhsLower, 2, 3, 0, 1);
            if((VectorMaskBits(VectorCompareGT(XMMLhsUpper, XMMRhs)) & 0x03) != 0x03)
                return false;

            return true;
        }
        bool Overlap(const AABB2D& AABB)const{
            VectorType XMMLhsLower = VectorLoadAligned(&LowerUpper);
            VectorType XMMRhsLower = VectorLoadAligned(&AABB.LowerUpper);
            VectorType XMMLhsUpper = VectorSwizzle(XMMLhsLower, 2, 3, 0, 1);
            VectorType XMMRhsUpper = VectorSwizzle(XMMRhsLower, 2, 3, 0, 1);
            
            if((VectorMaskBits(VectorCompareGT(XMMRhsLower, XMMLhsUpper)) & 0x03) != 0)
                return false;
            if((VectorMaskBits(VectorCompareGT(XMMLhsLower, XMMRhsUpper)) & 0x03) != 0)
                return false;

            return true;
        }

        FloatType GetPerimeter()const{
            VectorType XMMLower = VectorLoadAligned(&LowerUpper);
            VectorType XMMUpper = VectorSwizzle(XMMLower, 2, 3, 0, 1);

            VectorType XMMDiff = VectorSubtract(XMMUpper, XMMLower);

            VectorType XMMRet = VectorSwizzle(XMMDiff, 1, 0, 2, 3);
            XMMRet = VectorAdd(XMMDiff, XMMRet);
            XMMRet = VectorMultiply(VectorTwo<FloatType>, XMMRet);

            return VectorGetComponent(XMMRet, 0);
        }
    };
    template<typename FloatType>
    void CopyAABB(AABB2D<FloatType>* Lhs, const AABB2D<FloatType>* Rhs){
        typename AABB2D<FloatType>::VectorType XMMLowerUpper = VectorLoadAligned(&Rhs->LowerUpper);

        VectorStoreAligned(XMMLowerUpper, &Lhs->LowerUpper);
    }
    template<typename FloatType>
    bool operator==(const AABB2D<FloatType>& Lhs, const AABB2D<FloatType>& Rhs){
        typename AABB2D<FloatType>::VectorType XMMLhsLowerUpper = VectorLoadAligned(&Lhs.LowerUpper);
        typename AABB2D<FloatType>::VectorType XMMRhsLowerUpper = VectorLoadAligned(&Rhs.LowerUpper);

        typename AABB2D<FloatType>::VectorType XMMDiff = VectorSubtract(XMMLhsLowerUpper, XMMRhsLowerUpper);
        XMMDiff = VectorAbs(XMMDiff);
        if((VectorMaskBits(VectorCompareGT(XMMDiff, VectorKindaSmallNumber<FloatType>)) & 0x0f) != 0)
            return false;

        return true;
    }
    template<typename FloatType>
    constexpr UE::Math::TVector2<FloatType> AABB2DEpsilon(){ return UE::Math::TVector2<FloatType>(); }
    template<>
    constexpr UE::Math::TVector2<float> AABB2DEpsilon(){ return UE::Math::TVector2<float>(0.0001f, 0.0001f); }
    template<>
    constexpr UE::Math::TVector2<double> AABB2DEpsilon(){ return UE::Math::TVector2<double>(0.0001, 0.0001); }
    template<typename FloatType>
    constexpr AABB2D<FloatType> AABB2DError(){ return AABB2D<FloatType>{ UE::Math::TVector4<FloatType>(-FloatMax<FloatType>, -FloatMax<FloatType>, -FloatMax<FloatType>, -FloatMax<FloatType>) }; }

    template<typename FloatType>
    struct MS_ALIGN(16) AABB{
        typedef UE::Math::TVector<FloatType> Type;
        typedef typename VectorRegisterFinder<FloatType>::Type VectorType;
        
        
        UE::Math::TVector4<FloatType> Lower;
        UE::Math::TVector4<FloatType> Upper;


        void Expand(FloatType Expand){
            Lower.X -= Expand;
            Lower.Y -= Expand;
            Lower.Z -= Expand;
            Upper.X += Expand;
            Upper.Y += Expand;
            Upper.Z += Expand;
            
            ensure(Lower.X <= Upper.X);
            ensure(Lower.Y <= Upper.Y);
            ensure(Lower.Z <= Upper.Z);
        }
        static AABB Expand(const AABB& Rhs, FloatType Expand){
            AABB Ret = Rhs;
            Ret.Expand(Expand);
            return Ret;
        }

        void ExpandBySign(const Type& D){
            if(D.X < 0.f)
                Lower.X += D.X;
            else
                Upper.X += D.X;
            if(D.Y < 0.f)
                Lower.Y += D.Y;
            else
                Upper.Y += D.Y;
            if(D.Z < 0.f)
                Lower.Z += D.Z;
            else
                Upper.Z += D.Z;

            ensure(Lower.X <= Upper.X);
            ensure(Lower.Y <= Upper.Y);
            ensure(Lower.Z <= Upper.Z);
        }
        static AABB ExpandBySign(const AABB& Rhs, const Type& D){
            AABB Ret = Rhs;
            Ret.ExpandBySign(D);
            return Ret;
        }

        void Move(const Type& V){
            Lower += V;
            Upper += V;

            ensure(Lower.X <= Upper.X);
            ensure(Lower.Y <= Upper.Y);
            ensure(Lower.Z <= Upper.Z);
        }
        static AABB Move(const AABB& Rhs, const Type& V){
            AABB Ret = Rhs;
            Ret.Move(V);
            return Ret;
        }
        
        void Combine(const AABB& AABB){
            VectorType XMMLhsLower = VectorLoadAligned(&Lower);
            VectorType XMMLhsUpper = VectorLoadAligned(&Upper);
            VectorType XMMRhsLower = VectorLoadAligned(&AABB.Lower);
            VectorType XMMRhsUpper = VectorLoadAligned(&AABB.Upper);

            VectorType XMMLower = VectorMin(XMMLhsLower, XMMRhsLower);
            VectorType XMMUpper = VectorMax(XMMLhsUpper, XMMRhsUpper);

            ensure((VectorMaskBits(VectorCompareLE(XMMLower, XMMUpper)) & 0x07) == 0x07);

            VectorStoreAligned(XMMLower, &Lower);
            VectorStoreAligned(XMMUpper, &Upper);
        }
        void Combine(const AABB& Lhs, const AABB& Rhs){
            VectorType XMMLhsLower = VectorLoadAligned(&Lhs.Lower);
            VectorType XMMLhsUpper = VectorLoadAligned(&Lhs.Upper);
            VectorType XMMRhsLower = VectorLoadAligned(&Rhs.Lower);
            VectorType XMMRhsUpper = VectorLoadAligned(&Rhs.Upper);

            VectorType XMMLower = VectorMin(XMMLhsLower, XMMRhsLower);
            VectorType XMMUpper = VectorMax(XMMLhsUpper, XMMRhsUpper);

            ensure((VectorMaskBits(VectorCompareLE(XMMLower, XMMUpper)) & 0x07) == 0x07);

            VectorStoreAligned(XMMLower, &Lower);
            VectorStoreAligned(XMMUpper, &Upper);
        }

        bool Contains(const AABB& AABB)const{
            VectorType XMMLhsLower = VectorLoadAligned(&Lower);
            VectorType XMMRhsLower = VectorLoadAligned(&AABB.Lower);
            if((VectorMaskBits(VectorCompareGT(XMMRhsLower, XMMLhsLower)) & 0x07) != 0)
                return false;

            VectorType XMMLhsUpper = VectorLoadAligned(&Upper);
            VectorType XMMRhsUpper = VectorLoadAligned(&AABB.Upper);
            if((VectorMaskBits(VectorCompareGT(XMMLhsUpper, XMMRhsUpper)) & 0x07) != 0)
                return false;

            return true;
        }
        bool Contains(const UE::Math::TVector4<FloatType>& Point)const{
            VectorType XMMRhs = VectorLoadAligned(&Point);

            VectorType XMMLhsLower = VectorLoadAligned(&Lower);
            if((VectorMaskBits(VectorCompareGT(XMMRhs, XMMLhsLower)) & 0x07) != 0x07)
                return false;

            VectorType XMMLhsUpper = VectorLoadAligned(&Upper);
            if((VectorMaskBits(VectorCompareGT(XMMLhsUpper, XMMRhs)) & 0x07) != 0x07)
                return false;

            return true;
        }
        bool Overlap(const AABB& AABB)const{
            VectorType XMMRhsLower = VectorLoadAligned(&AABB.Lower);
            VectorType XMMLhsUpper = VectorLoadAligned(&Upper);
            if((VectorMaskBits(VectorCompareGT(XMMRhsLower, XMMLhsUpper)) & 0x07) != 0)
                return false;

            VectorType XMMLhsLower = VectorLoadAligned(&Lower);
            VectorType XMMRhsUpper = VectorLoadAligned(&AABB.Upper);
            if((VectorMaskBits(VectorCompareGT(XMMLhsLower, XMMRhsUpper)) & 0x07) != 0)
                return false;

            return true;
        }

        FloatType GetPerimeter()const{
            VectorType XMMLower = VectorLoadAligned(&Lower);
            VectorType XMMUpper = VectorLoadAligned(&Upper);

            VectorType XMMDiff = VectorSubtract(XMMUpper, XMMLower);
            VectorType XMMSRet = VectorDot3(VectorTwo<FloatType>, XMMDiff);

            return VectorGetComponent(XMMSRet, 0);
        }
    };
    template<typename FloatType>
    void CopyAABB(AABB<FloatType>* Lhs, const AABB<FloatType>* Rhs){
        typename AABB<FloatType>::VectorType XMMLower = VectorLoadAligned(&Rhs->Lower);
        typename AABB<FloatType>::VectorType XMMUpper = VectorLoadAligned(&Rhs->Upper);

        VectorStoreAligned(XMMLower, &Lhs->Lower);
        VectorStoreAligned(XMMUpper, &Lhs->Upper);
    }
    template<typename FloatType>
    bool operator==(const AABB<FloatType>& lhs, const AABB<FloatType>& rhs){
        typename AABB<FloatType>::VectorType XMMLhsLower = VectorLoadAligned(&lhs.Lower);
        typename AABB<FloatType>::VectorType XMMLhsUpper = VectorLoadAligned(&lhs.Upper);
        typename AABB<FloatType>::VectorType XMMRhsLower = VectorLoadAligned(&rhs.Lower);
        typename AABB<FloatType>::VectorType XMMRhsUpper = VectorLoadAligned(&rhs.Upper);

        typename AABB<FloatType>::VectorType XMMDiff = VectorSubtract(XMMLhsLower, XMMRhsLower);
        XMMDiff = VectorAbs(XMMDiff);
        if((VectorMaskBits(VectorCompareGT(XMMDiff, VectorKindaSmallNumber<FloatType>)) & 0x07) != 0)
            return false;

        XMMDiff = VectorSubtract(XMMLhsUpper, XMMRhsUpper);
        XMMDiff = VectorAbs(XMMDiff);
        if((VectorMaskBits(VectorCompareGT(XMMDiff, VectorKindaSmallNumber<FloatType>)) & 0x07) != 0)
            return false;

        return true;
    }
    template<typename FloatType>
    constexpr UE::Math::TVector<FloatType> AABBEpsilon(){ return UE::Math::TVector<FloatType>(); }
    template<>
    constexpr UE::Math::TVector<float> AABBEpsilon(){ return UE::Math::TVector<float>(0.0001f, 0.0001f, 0.0001f); }
    template<>
    constexpr UE::Math::TVector<double> AABBEpsilon(){ return UE::Math::TVector<double>(0.0001, 0.0001, 0.0001); }
    template<typename FloatType>
    constexpr AABB<FloatType> AABBError(){ return AABB<FloatType>{ UE::Math::TVector4<FloatType>(-FloatMax<FloatType>, -FloatMax<FloatType>, -FloatMax<FloatType>, -FloatMax<FloatType>), UE::Math::TVector4<FloatType>(-FloatMax<FloatType>, -FloatMax<FloatType>, -FloatMax<FloatType>, -FloatMax<FloatType>) }; }

    template<typename SizeType, typename BoundType>
    struct MS_ALIGN(16) Node{
        union{
            SizeType Parent;
            SizeType Next;
        };
        SizeType Child1;
        SizeType Child2;

        // leaf = 0, free = -1
        SizeType Height;

        BoundType Bound;

        bool bMoved;


        bool IsLeaf()const{ return (Child1 == NodeNull<SizeType>); }
    };

    template<typename InElementType, typename InAllocator, int32 N>
    class QueryStack{
    public:
        typedef typename InAllocator::SizeType SizeType;
        typedef InElementType ElementType;
        typedef InAllocator Allocator;


    public:
        QueryStack(){
            Stack = Array;
            Count = 0;
            Capacity = static_cast<SizeType>(N);
        }
        ~QueryStack(){
            if(Stack != Array){
                Allocator::Deallocate(Stack);
                Stack = nullptr;
            }
        }


    public:
        void Push(const ElementType& element){
            if(Count == Capacity){
                ElementType* Old = Stack;
                Capacity <<= 1;
                Stack = reinterpret_cast<ElementType*>(Allocator::Allocate(Capacity * sizeof(ElementType), 0));
                memcpy_s(Stack, Capacity * sizeof(ElementType), Old, Count * sizeof(ElementType));
                if(Old != Array)
                    Allocator::Deallocate(Old);
            }

            Stack[Count] = element;
            ++Count;
        }
        ElementType Pop(){
            check(Count > 0);
            --Count;
            return Stack[Count];
        }

    public:
        SizeType GetCount()const{ return Count; }


    private:
        ElementType* Stack;
        ElementType Array[N];
        SizeType Count;
        SizeType Capacity;
    };
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<typename SizeType>
struct TBVHBound2D : __hidden_DynamicBVH::AABB2D<SizeType>{
    static const UE::Math::TVector2<SizeType> Epsilon;
    static const TBVHBound2D Error;


    TBVHBound2D(){
        LowerUpper = UE::Math::TVector4<SizeType>::Zero();
    }
    TBVHBound2D(const UE::Math::TVector2<SizeType>& Point){
        LowerUpper.X = Point.X - Epsilon.X;
        LowerUpper.Y = Point.X - Epsilon.Y;
        LowerUpper.Z = Point.X + Epsilon.X;
        LowerUpper.W = Point.X + Epsilon.Y;

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
    TBVHBound2D(const UE::Math::TVector2<SizeType>& Point, SizeType Radius){
        LowerUpper.X = Point.X - Radius;
        LowerUpper.Y = Point.X - Radius;
        LowerUpper.Z = Point.X + Radius;
        LowerUpper.W = Point.X + Radius;

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
    TBVHBound2D(const UE::Math::TVector2<SizeType>& _Lower, const UE::Math::TVector2<SizeType>& _Upper){
        LowerUpper.X = _Lower.X;
        LowerUpper.Y = _Lower.Y;
        LowerUpper.Z = _Upper.X;
        LowerUpper.W = _Upper.Y;

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
    TBVHBound2D(const UE::Math::TVector2<SizeType>& Point0, const UE::Math::TVector2<SizeType>& Point1, const UE::Math::TVector2<SizeType>& Point2){
        LowerUpper.X = FMath::Min3(Point0.X, Point1.X, Point2.X);
        LowerUpper.Y = FMath::Min3(Point0.Y, Point1.Y, Point2.X);
        LowerUpper.Z = FMath::Max3(Point0.X, Point1.X, Point2.X);
        LowerUpper.W = FMath::Max3(Point0.Y, Point1.Y, Point2.X);

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
    TBVHBound2D(const UE::Math::TVector4<SizeType>& _LowerUpper){
        LowerUpper = _LowerUpper;

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
    TBVHBound2D(const __hidden_DynamicBVH::AABB2D<SizeType>& AABB){
        LowerUpper = AABB.LowerUpper;

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
    TBVHBound2D(const UE::Math::TBox2<SizeType>& AABB){
        LowerUpper.X = AABB.Min.X;
        LowerUpper.Y = AABB.Min.Y;
        LowerUpper.Z = AABB.Max.X;
        LowerUpper.W = AABB.Max.Y;

        ensure(LowerUpper.X <= LowerUpper.Z);
        ensure(LowerUpper.Y <= LowerUpper.W);
    }
};
template<typename SizeType>
const UE::Math::TVector2<SizeType> TBVHBound2D<SizeType>::Epsilon = __hidden_DynamicBVH::AABB2DEpsilon<SizeType>();
template<typename SizeType>
const TBVHBound2D<SizeType> TBVHBound2D<SizeType>::Error = __hidden_DynamicBVH::AABB2DError<SizeType>();

template<typename SizeType>
struct TBVHBound : __hidden_DynamicBVH::AABB<SizeType>{
    static const UE::Math::TVector<SizeType> Epsilon;
    static const TBVHBound Error;


    TBVHBound(){
        Lower = UE::Math::TVector<SizeType>::Zero();
        Upper = UE::Math::TVector<SizeType>::Zero();
    }
    TBVHBound(const UE::Math::TVector<SizeType>& Point){
        Lower = Point - Epsilon;
        Upper = Point + Epsilon;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const UE::Math::TVector4<SizeType>& Point){
        Lower = UE::Math::TVector4<SizeType>(Point) - Epsilon;
        Upper = UE::Math::TVector4<SizeType>(Point) + Epsilon;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const UE::Math::TVector4<SizeType>& Point, SizeType Radius){
        Lower = UE::Math::TVector<SizeType>(Point) - Radius;
        Upper = UE::Math::TVector<SizeType>(Point) + Radius;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const UE::Math::TVector<SizeType>& _Lower, const UE::Math::TVector<SizeType>& _Upper){
        Lower = _Lower;
        Upper = _Upper;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const UE::Math::TVector4<SizeType>& _Lower, const UE::Math::TVector4<SizeType>& _Upper){
        Lower = _Lower;
        Upper = _Upper;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const __hidden_DynamicBVH::AABB<SizeType>& AABB){
        Lower = AABB.Lower;
        Upper = AABB.Upper;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const UE::Math::TBox<SizeType>& AABB){
        Lower = AABB.Min;
        Upper = AABB.Max;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
    TBVHBound(const UE::Math::TBoxSphereBounds<SizeType, SizeType>& Bound){
        Lower = Bound.Origin - Bound.BoxExtent;
        Upper = Bound.Origin + Bound.BoxExtent;

        ensure(Lower.X <= Upper.X);
        ensure(Lower.Y <= Upper.Y);
        ensure(Lower.Z <= Upper.Z);
    }
};
template<typename SizeType>
const UE::Math::TVector<SizeType> TBVHBound<SizeType>::Epsilon = __hidden_DynamicBVH::AABBEpsilon<SizeType>();
template<typename SizeType>
const TBVHBound<SizeType> TBVHBound<SizeType>::Error = __hidden_DynamicBVH::AABBError<SizeType>();


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct TBVHAllocator{
    typedef FDefaultSetAllocator DataAllocator;
    typedef int32 SizeType;
    typedef double FloatType;

    static void* Allocate(SizeType Size, SizeType Align){ return FMemory::Malloc(Size, Align); }
    static void Deallocate(void* P){ FMemory::Free(P); }
};

template<typename InElementType, typename InAllocator = TBVHAllocator, typename BoundType = TBVHBound<typename InAllocator::FloatType>, int32 QueryStackCapacity = 1 << 11>
class TDynamicBVH{
public:
    typedef typename InAllocator::SizeType SizeType;
    typedef typename InAllocator::FloatType FloatType;
    typedef InElementType ElementType;
    typedef __hidden_DynamicBVH::Node<SizeType, BoundType> NodeType;
    typedef TMap<SizeType, ElementType, typename InAllocator::DataAllocator> DataContainer;
    typedef InAllocator Allocator;


public:
    TDynamicBVH(int32 InitNodeCapacity = 4096){
        Root = __hidden_DynamicBVH::NodeNull<SizeType>;

        NodeCapacity = static_cast<SizeType>(InitNodeCapacity);
        NodeCount = 0;

        Nodes = reinterpret_cast<NodeType*>(Allocator::Allocate(NodeCapacity * sizeof(NodeType), 16));
        memset(Nodes, 0, NodeCapacity * sizeof(NodeType));

        for(SizeType i = 0; i < NodeCapacity - 1; ++i){
            Nodes[i].Next = i + 1;
            Nodes[i].Height = static_cast<SizeType>(-1);
        }
        Nodes[NodeCapacity - 1].Next = __hidden_DynamicBVH::NodeNull<SizeType>;
        Nodes[NodeCapacity - 1].Height = static_cast<SizeType>(-1);
        FreeList = 0;

        InsertionCount = 0;

        Data.Reserve(InitNodeCapacity);
    }
    TDynamicBVH(const TDynamicBVH& Other){
        Root = Other.Root;

        NodeCapacity = Other.NodeCapacity;
        NodeCount = Other.NodeCount;

        Nodes = reinterpret_cast<NodeType*>(Allocator::Allocate(NodeCapacity * sizeof(NodeType), 16));
        for(SizeType i = 0; i < NodeCapacity; ++i){
            Nodes[i].Parent = Other.Nodes[i].Parent;

            Nodes[i].Child1 = Other.Nodes[i].Child1;
            Nodes[i].Child2 = Other.Nodes[i].Child2;

            Nodes[i].Height = Other.Nodes[i].Height;

            __hidden_DynamicBVH::CopyAABB(&Nodes[i].Bound, &Other.Nodes[i].Bound);

            Nodes[i].bMoved = Other.Nodes[i].bMoved;
        }

        FreeList = Other.FreeList;
        InsertionCount = Other.InsertionCount;

        Data = Other.Data;
    }
    TDynamicBVH(TDynamicBVH&& Other)noexcept{
        Root = Other.Root;
        Other.Root = __hidden_DynamicBVH::NodeNull<SizeType>;

        NodeCapacity = Other.NodeCapacity;
        Other.NodeCapacity = 0;

        NodeCount = Other.NodeCount;
        Other.NodeCount = 0;

        Nodes = Other.Nodes;
        Other.Nodes = nullptr;

        FreeList = Other.FreeList;
        Other.FreeList = 0;

        InsertionCount = Other.InsertionCount;
        Other.InsertionCount = 0;

        Data = MoveTemp(Other.Data);
    }
    ~TDynamicBVH(){
        if(Nodes)
            Allocator::Deallocate(Nodes);
    }

public:
    TDynamicBVH& operator=(const TDynamicBVH& Other){
        Root = Other.Root;

        // it needs optimization
        if(NodeCapacity != Other.NodeCapacity){
            if(Nodes){
                Allocator::Deallocate(Nodes);
                Nodes = nullptr;
            }
        }
        NodeCapacity = Other.NodeCapacity;
        NodeCount = Other.NodeCount;

        if(!Nodes)
            Nodes = reinterpret_cast<NodeType*>(Allocator::Allocate(NodeCapacity * sizeof(NodeType), 16));
        for(SizeType i = 0; i < NodeCapacity; ++i){
            Nodes[i].Parent = Other.Nodes[i].Parent;

            Nodes[i].Child1 = Other.Nodes[i].Child1;
            Nodes[i].Child2 = Other.Nodes[i].Child2;

            Nodes[i].Height = Other.Nodes[i].Height;

            __hidden_DynamicBVH::CopyAABB(&Nodes[i].Bound, &Other.Nodes[i].Bound);

            Nodes[i].bMoved = Other.Nodes[i].bMoved;
        }

        FreeList = Other.FreeList;
        InsertionCount = Other.InsertionCount;

        Data = Other.Data;

        return *this;
    }
    TDynamicBVH& operator=(TDynamicBVH&& Other)noexcept{
        if(Nodes)
            Allocator::Deallocate(Nodes);

        Root = Other.Root;
        Other.Root = __hidden_DynamicBVH::NodeNull<SizeType>;

        NodeCapacity = Other.NodeCapacity;
        Other.NodeCapacity = 0;

        NodeCount = Other.NodeCount;
        Other.NodeCount = 0;

        Nodes = Other.Nodes;
        Other.Nodes = nullptr;

        FreeList = Other.FreeList;
        Other.FreeList = 0;

        InsertionCount = Other.InsertionCount;
        Other.InsertionCount = 0;

        Data = MoveTemp(Other.Data);

        return *this;
    }


public:
    void Reset(){
        Root = __hidden_DynamicBVH::NodeNull<SizeType>;

        if(NodeCapacity < 2)
            NodeCapacity = 256;
        NodeCount = 0;

        if(!Nodes)
            Nodes = reinterpret_cast<NodeType*>(Allocator::Allocate(NodeCapacity * sizeof(NodeType), 16));
        memset(Nodes, 0, NodeCapacity * sizeof(NodeType));

        for(SizeType i = 0; i < NodeCapacity - 1; ++i){
            Nodes[i].Next = i + 1;
            Nodes[i].Height = static_cast<SizeType>(-1);
        }
        Nodes[NodeCapacity - 1].Next = __hidden_DynamicBVH::NodeNull<SizeType>;
        Nodes[NodeCapacity - 1].Height = static_cast<SizeType>(-1);
        FreeList = 0;

        InsertionCount = 0;

        Data.Reset();
        Data.Reserve(NodeCapacity);
    }

public:
    template<typename... ARGS>
    SizeType CreateProxy(const BoundType& Bound, ARGS&&... Args){
        SizeType ProxyID = AllocateNode();
        
        Nodes[ProxyID].Bound = BoundType::Expand(Bound, __hidden_DynamicBVH::FatExtension);
        Nodes[ProxyID].Height = 0;
        Nodes[ProxyID].bMoved = true;

        if(ElementType* Exist = Data.Find(ProxyID)){
            (*Exist) = ElementType(Forward<ARGS>(Args)...);
        }
        else
            Data.Emplace(ProxyID, Forward<ARGS>(Args)...);

        InsertLeaf(ProxyID);
        return ProxyID;
    }
    void DestroyProxy(SizeType ProxyID){
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        check(Nodes[ProxyID].IsLeaf());

        RemoveLeaf(ProxyID);

        if(Nodes[ProxyID].IsLeaf())
            Data.Remove(ProxyID);

        FreeNode(ProxyID);
    }

    int8 MoveProxy(SizeType ProxyID, const BoundType& Bound, const typename BoundType::Type& Displacement = BoundType::Type::Zero()){
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        check(Nodes[ProxyID].IsLeaf());

        BoundType FatBound = BoundType::Expand(Bound, __hidden_DynamicBVH::FatExtension);

        const BoundType& TreeBound = Nodes[ProxyID].Bound;
        if(TreeBound == FatBound)
            return 0;

        const typename BoundType::Type D = __hidden_DynamicBVH::AABBMultiplier * Displacement;
        FatBound.ExpandBySign(D);

        if(TreeBound.Contains(Bound)){
            BoundType HugeBound = BoundType::Expand(FatBound, 4 * __hidden_DynamicBVH::FatExtension);
            if(HugeBound.Contains(TreeBound))
                return -1;
        }

        RemoveLeaf(ProxyID);
        Nodes[ProxyID].Bound = FatBound;

        InsertLeaf(ProxyID);
        Nodes[ProxyID].bMoved = true;

        return 1;
    }

public:
    ElementType* GetData(SizeType ProxyID){
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        if(Nodes[ProxyID].IsLeaf())
            return Data.Find(ProxyID);
        return nullptr;
    }
    const ElementType* GetData(SizeType ProxyID)const{
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        if(Nodes[ProxyID].IsLeaf())
            return Data.Find(ProxyID);
        return nullptr;
    }

public:
    bool WasMoved(SizeType ProxyID)const{
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        return Nodes[ProxyID].bMoved;
    }
    void ClearMoved(SizeType ProxyID){
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        Nodes[ProxyID].bMoved = false;
    }

    const BoundType& GetFatBound(SizeType ProxyID)const{
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        return Nodes[ProxyID].Bound;
    }
    BoundType GetBound(SizeType ProxyID)const{
        check(0 <= ProxyID && ProxyID < NodeCapacity);
        
        BoundType Bound = BoundType::Expand(Nodes[ProxyID].Bound, -__hidden_DynamicBVH::FatExtension);

        return Bound;
    }

public:
    typename DataContainer::TRangedForIterator begin(){ return Data.begin(); }
    typename DataContainer::TRangedForConstIterator begin()const{ return Data.begin(); }
    typename DataContainer::TRangedForIterator end(){ return Data.end(); }
    typename DataContainer::TRangedForConstIterator end()const{ return Data.end(); }

public:
    template<typename FUNC>
    void Iterate(FUNC&& Callback){
        for(auto& Wrapped : Data){
            Callback(Wrapped.template Get<0>(), Wrapped.template Get<1>());
        }
    }
    template<typename FUNC>
    void IterateConst(FUNC&& Callback)const{
        for(const auto& Wrapped : Data){
            Callback(Wrapped.template Get<0>(), Wrapped.template Get<1>());
        }
    }
    template<typename FUNC>
    void BreakableIterate(FUNC&& Callback){
        for(auto& Wrapped : Data){
            if(!Callback(Wrapped.template Get<0>(), Wrapped.template Get<1>()))
                return;
        }
    }
    template<typename FUNC>
    void BreakableIterateConst(FUNC&& Callback)const{
        for(const auto& Wrapped : Data){
            if(!Callback(Wrapped.template Get<0>(), Wrapped.template Get<1>()))
                return;
        }
    }

    template<typename COLLCHECK, typename FUNC>
    void Query(COLLCHECK&& CollCheck, FUNC&& Callback){
        __hidden_DynamicBVH::QueryStack<SizeType, Allocator, QueryStackCapacity> Stack;
        Stack.Push(Root);

        while(Stack.GetCount() > 0){
            SizeType NodeID = Stack.Pop();
            if(NodeID == __hidden_DynamicBVH::NodeNull<SizeType>)
                continue;

            NodeType* Node = Nodes + NodeID;

            if(Node->IsLeaf()){
                ElementType* Exist = Data.Find(NodeID);
                if(Exist){
                    if(CollCheck(Node->Bound))
                        Callback(NodeID, *Exist);
                }
                else
                    ensure(Exist != nullptr);
            }
            else if(CollCheck(Node->Bound)){
                Stack.Push(Node->Child1);
                Stack.Push(Node->Child2);
            }
        }
    }
    template<typename COLLCHECK, typename FUNC>
    void QueryConst(COLLCHECK&& CollCheck, FUNC&& Callback)const{
        __hidden_DynamicBVH::QueryStack<SizeType, Allocator, QueryStackCapacity> Stack;
        Stack.Push(Root);

        while(Stack.GetCount() > 0){
            SizeType NodeID = Stack.Pop();
            if(NodeID == __hidden_DynamicBVH::NodeNull<SizeType>)
                continue;

            const NodeType* Node = Nodes + NodeID;

            if(Node->IsLeaf()){
                const ElementType* Exist = Data.Find(NodeID);
                if(Exist){
                    if(CollCheck(Node->Bound))
                        Callback(NodeID, *Exist);
                }
                else
                    ensure(Exist != nullptr);
            }
            else if(CollCheck(Node->Bound)){
                Stack.Push(Node->Child1);
                Stack.Push(Node->Child2);
            }
        }
    }
    template<typename COLLCHECK, typename FUNC>
    void BreakableQuery(COLLCHECK&& CollCheck, FUNC&& Callback){
        __hidden_DynamicBVH::QueryStack<SizeType, Allocator, QueryStackCapacity> Stack;
        Stack.Push(Root);

        while(Stack.GetCount() > 0){
            SizeType NodeID = Stack.Pop();
            if(NodeID == __hidden_DynamicBVH::NodeNull<SizeType>)
                continue;

            NodeType* Node = Nodes + NodeID;

            if(Node->IsLeaf()){
                ElementType* Exist = Data.Find(NodeID);
                if(Exist){
                    if(CollCheck(Node->Bound)){
                        if(!Callback(NodeID, *Exist))
                            return;
                    }
                }
                else
                    ensure(Exist != nullptr);
            }
            else if(CollCheck(Node->Bound)){
                Stack.Push(Node->Child1);
                Stack.Push(Node->Child2);
            }
        }
    }
    template<typename COLLCHECK, typename FUNC>
    void BreakableQueryConst(COLLCHECK&& CollCheck, FUNC&& Callback)const{
        __hidden_DynamicBVH::QueryStack<SizeType, Allocator, QueryStackCapacity> Stack;
        Stack.Push(Root);

        while(Stack.GetCount() > 0){
            SizeType NodeID = Stack.Pop();
            if(NodeID == __hidden_DynamicBVH::NodeNull<SizeType>)
                continue;

            const NodeType* Node = Nodes + NodeID;

            if(Node->IsLeaf()){
                const ElementType* Exist = Data.Find(NodeID);
                if(Exist){
                    if(CollCheck(Node->Bound)){
                        if(!Callback(NodeID, *Exist))
                            return;
                    }
                }
                else
                    ensure(Exist != nullptr);
            }
            else if(CollCheck(Node->Bound)){
                Stack.Push(Node->Child1);
                Stack.Push(Node->Child2);
            }
        }
    }

public:
    void Validate()const{
        ValidateStructure(Root);
        ValidateMetrics(Root);

        SizeType FreeCount = 0;
        SizeType FreeIndex = FreeList;
        while(FreeIndex != __hidden_DynamicBVH::NodeNull<SizeType>){
            check(0 <= FreeIndex && FreeIndex < NodeCapacity);
            FreeIndex = Nodes[FreeIndex].Next;
            ++FreeCount;
        }

        check(GetHeight() == ComputeHeight());
        check(NodeCount + FreeCount == NodeCapacity);
    }

public:
    SizeType NumLeaves()const{ return Data.Num(); }

    SizeType GetHeight()const{
        if(Root == __hidden_DynamicBVH::NodeNull<SizeType>)
            return 0;

        return Nodes[Root].Height;
    }
    SizeType GetMaxBalance()const{
        SizeType MaxBalance = 0;
        for(SizeType i = 0; i < NodeCapacity; ++i){
            const NodeType* Node = Nodes + i;
            if(Node->Height <= 1)
                continue;

            check(Node->IsLeaf() == false);

            SizeType Child1 = Node->Child1;
            SizeType Child2 = Node->Child2;
            SizeType Balance = FMath::Abs(Nodes[Child2].Height - Nodes[Child2].Height);
            MaxBalance = FMath::Max(MaxBalance, Balance);
        }
        return MaxBalance;
    }

    FloatType GetAreaRatio()const{
        if(Root == __hidden_DynamicBVH::NodeNull<SizeType>)
            return 0;

        const NodeType* Root = Nodes + Root;
        const FloatType RootArea = Root->Bound.GetPerimeter();

        FloatType TotalArea = 0;
        for(SizeType i = 0; i < NodeCapacity; ++i){
            const NodeType* Node = Nodes + i;
            if(Node->Height < 0)
                continue;

            TotalArea += Node->Bound.GetPerimeter();
        }

        return TotalArea / RootArea;
    }

public:
    void RebuildBottomUp(){
        SizeType* Nodes = reinterpret_cast<SizeType*>(Allocator::Allocate(NodeCount * sizeof(SizeType), 0));
        SizeType Count = 0;

        for(SizeType i = 0; i < NodeCapacity; ++i){
            if(Nodes[i].Height < 0)
                continue;

            if(Nodes[i].IsLeaf()){
                Nodes[i].Parent = __hidden_DynamicBVH::NodeNull<SizeType>;
                Nodes[Count] = i;
                ++Count;
            }
            else
                FreeNode(i);
        }

        while(Count > 1){
            FloatType MinCost = __hidden_DynamicBVH::FloatMax<FloatType>;
            SizeType iMin = static_cast<SizeType>(-1), jMin = static_cast<SizeType>(-1);
            for(SizeType i = 0; i < Count; ++i){
                BoundType BoundI = Nodes[Nodes[i]].Bound;

                for(SizeType j = i + 1; j < Count; ++j){
                    BoundType BoundJ = Nodes[Nodes[j]].Bound;

                    BoundType B;
                    B.Combine(BoundI, BoundJ);

                    const FloatType Cost = B.GetPerimeter();
                    if(Cost < MinCost){
                        iMin = i;
                        jMin = j;
                        MinCost = Cost;
                    }
                }
            }

            SizeType Index1 = Nodes[iMin];
            SizeType Index2 = Nodes[jMin];
            NodeType* Child1 = Nodes + Index1;
            NodeType* Child2 = Nodes + Index2;

            SizeType ParentIndex = AllocateNode();
            NodeType* Parent = Nodes + ParentIndex;
            Parent->Child1 = Index1;
            Parent->Child2 = Index2;
            Parent->Height = 1 + FMath::Max(Child1->Height, Child2->Height);
            Parent->Bound.Combine(Child1->Bound, Child2->Bound);
            Parent->Parent = __hidden_DynamicBVH::NodeNull<SizeType>;

            Child1->Parent = ParentIndex;
            Child2->Parent = ParentIndex;

            Nodes[jMin] = Nodes[Count - 1];
            Nodes[iMin] = ParentIndex;
            --Count;
        }

        Root = Nodes[0];
        Allocator::Deallocate(Nodes);

        // Validate();
    }

    void ShiftOrigin(const typename BoundType::Type& NewOrigin){
        for(SizeType i = 0; i < NodeCapacity; ++i)
            Nodes[i].Bound.Move(-NewOrigin);
    }


private:
    SizeType AllocateNode(){
        if(FreeList == __hidden_DynamicBVH::NodeNull<SizeType>){
            check(NodeCount == NodeCapacity);

            NodeType* OldNodes = Nodes;
            NodeCapacity <<= 1;
            Nodes = reinterpret_cast<NodeType*>(Allocator::Allocate(NodeCapacity * sizeof(NodeType), 16));
            for(SizeType i = 0; i < NodeCount; ++i)
                Nodes[i] = MoveTemp(OldNodes[i]);
            Allocator::Deallocate(OldNodes);

            memset(&Nodes[NodeCount], 0, (NodeCapacity - NodeCount) * sizeof(NodeType));
            for(SizeType i = NodeCount; i < NodeCapacity - 1; ++i){
                Nodes[i].Next = i + 1;
                Nodes[i].Height = -1;
            }
            Nodes[NodeCapacity - 1].Next = __hidden_DynamicBVH::NodeNull<SizeType>;
            Nodes[NodeCapacity - 1].Height = -1;
            FreeList = NodeCount;
        }

        SizeType NodeID = FreeList;
        FreeList = Nodes[NodeID].Next;
        Nodes[NodeID].Parent = __hidden_DynamicBVH::NodeNull<SizeType>;
        Nodes[NodeID].Child1 = __hidden_DynamicBVH::NodeNull<SizeType>;
        Nodes[NodeID].Child2 = __hidden_DynamicBVH::NodeNull<SizeType>;
        Nodes[NodeID].Height = 0;
        Nodes[NodeID].bMoved = false;
        ++NodeCount;
        return NodeID;
    }
    void FreeNode(SizeType NodeID){
        check(0 <= NodeID && NodeID < NodeCapacity);
        check(0 < NodeCount);
        Nodes[NodeID].Next = FreeList;
        Nodes[NodeID].Height = -1;
        FreeList = NodeID;
        --NodeCount;
    }

private:
    void InsertLeaf(SizeType Leaf){
        ++InsertionCount;

        if(Root == __hidden_DynamicBVH::NodeNull<SizeType>){
            Root = Leaf;
            Nodes[Root].Parent = __hidden_DynamicBVH::NodeNull<SizeType>;
            return;
        }

        BoundType LeafBound = Nodes[Leaf].Bound;
        SizeType Index = Root;
        while(!Nodes[Index].IsLeaf()){
            SizeType Child1 = Nodes[Index].Child1;
            SizeType Child2 = Nodes[Index].Child2;

            const FloatType Area = Nodes[Index].Bound.GetPerimeter();

            BoundType CombinedBound;
            CombinedBound.Combine(Nodes[Index].Bound, LeafBound);
            const FloatType CombinedArea = CombinedBound.GetPerimeter();

            const FloatType Cost = 2 * CombinedArea;
            const FloatType inheritanceCost = 2 * (CombinedArea - Area);

            FloatType Cost1;
            if(Nodes[Child1].IsLeaf()){
                BoundType Bound;
                Bound.Combine(LeafBound, Nodes[Child1].Bound);
                Cost1 = Bound.GetPerimeter() + inheritanceCost;
            }
            else{
                BoundType Bound;
                Bound.Combine(LeafBound, Nodes[Child1].Bound);
                const FloatType OldArea = Nodes[Child1].Bound.GetPerimeter();
                const FloatType NewArea = Bound.GetPerimeter();
                Cost1 = (NewArea - OldArea) + inheritanceCost;
            }

            FloatType Cost2;
            if(Nodes[Child2].IsLeaf()){
                BoundType Bound;
                Bound.Combine(LeafBound, Nodes[Child2].Bound);
                Cost2 = Bound.GetPerimeter() + inheritanceCost;
            }
            else{
                BoundType Bound;
                Bound.Combine(LeafBound, Nodes[Child2].Bound);
                const FloatType OldArea = Nodes[Child2].Bound.GetPerimeter();
                const FloatType NewArea = Bound.GetPerimeter();
                Cost2 = (NewArea - OldArea) + inheritanceCost;
            }

            if(Cost < Cost1 && Cost < Cost2)
                break;

            if(Cost1 < Cost2)
                Index = Child1;
            else
                Index = Child2;
        }

        SizeType Sibling = Index;

        SizeType OldParent = Nodes[Sibling].Parent;
        SizeType NewParent = AllocateNode();
        Nodes[NewParent].Parent = OldParent;
        Nodes[NewParent].Bound.Combine(LeafBound, Nodes[Sibling].Bound);
        Nodes[NewParent].Height = Nodes[Sibling].Height + 1;

        if(OldParent != __hidden_DynamicBVH::NodeNull<SizeType>){
            if(Nodes[OldParent].Child1 == Sibling)
                Nodes[OldParent].Child1 = NewParent;
            else
                Nodes[OldParent].Child2 = NewParent;

            Nodes[NewParent].Child1 = Sibling;
            Nodes[NewParent].Child2 = Leaf;
            Nodes[Sibling].Parent = NewParent;
            Nodes[Leaf].Parent = NewParent;
        }
        else{
            Nodes[NewParent].Child1 = Sibling;
            Nodes[NewParent].Child2 = Leaf;
            Nodes[Sibling].Parent = NewParent;
            Nodes[Leaf].Parent = NewParent;
            Root = NewParent;
        }

        Index = Nodes[Leaf].Parent;
        while(Index != __hidden_DynamicBVH::NodeNull<SizeType>){
            Index = Balance(Index);

            SizeType Child1 = Nodes[Index].Child1;
            SizeType Child2 = Nodes[Index].Child2;

            check(Child1 != __hidden_DynamicBVH::NodeNull<SizeType>);
            check(Child2 != __hidden_DynamicBVH::NodeNull<SizeType>);

            Nodes[Index].Height = 1 + FMath::Max(Nodes[Child1].Height, Nodes[Child2].Height);
            Nodes[Index].Bound.Combine(Nodes[Child1].Bound, Nodes[Child2].Bound);

            Index = Nodes[Index].Parent;
        }

        // Validate();
    }
    void RemoveLeaf(SizeType Leaf){
        if(Leaf == Root){
            Root = __hidden_DynamicBVH::NodeNull<SizeType>;
            return;
        }

        SizeType Parent = Nodes[Leaf].Parent;
        SizeType GrandParent = Nodes[Parent].Parent;
        SizeType Sibling;
        if(Nodes[Parent].Child1 == Leaf)
            Sibling = Nodes[Parent].Child2;
        else
            Sibling = Nodes[Parent].Child1;

        if(GrandParent != __hidden_DynamicBVH::NodeNull<SizeType>){
            if(Nodes[GrandParent].Child1 == Parent)
                Nodes[GrandParent].Child1 = Sibling;
            else
                Nodes[GrandParent].Child2 = Sibling;

            Nodes[Sibling].Parent = GrandParent;
            FreeNode(Parent);

            SizeType Index = GrandParent;
            while(Index != __hidden_DynamicBVH::NodeNull<SizeType>){
                Index = Balance(Index);

                SizeType Child1 = Nodes[Index].Child1;
                SizeType Child2 = Nodes[Index].Child2;
                
                Nodes[Index].Height = 1 + FMath::Max(Nodes[Child1].Height, Nodes[Child2].Height);
                Nodes[Index].Bound.Combine(Nodes[Child1].Bound, Nodes[Child2].Bound);

                Index = Nodes[Index].Parent;
            }
        }
        else{
            Root = Sibling;
            Nodes[Sibling].Parent = __hidden_DynamicBVH::NodeNull<SizeType>;
            FreeNode(Parent);
        }

        // Validate();
    }

private:
    SizeType Balance(SizeType iA){
        check(iA != __hidden_DynamicBVH::NodeNull<SizeType>);

        NodeType* A = Nodes + iA;
        if(A->IsLeaf() || A->Height < 2)
            return iA;

        SizeType iB = A->Child1;
        SizeType iC = A->Child2;
        check(0 <= iB && iB < NodeCapacity);
        check(0 <= iC && iC < NodeCapacity);

        NodeType* B = Nodes + iB;
        NodeType* C = Nodes + iC;

        SizeType Balance = C->Height - B->Height;

        if(Balance > 1){
            SizeType iF = C->Child1;
            SizeType iG = C->Child2;
            check(0 <= iF && iF < NodeCapacity);
            check(0 <= iG && iG < NodeCapacity);

            NodeType* F = Nodes + iF;
            NodeType* G = Nodes + iG;

            C->Child1 = iA;
            C->Parent = A->Parent;
            A->Parent = iC;

            if(C->Parent != __hidden_DynamicBVH::NodeNull<SizeType>){
                if(Nodes[C->Parent].Child1 == iA)
                    Nodes[C->Parent].Child1 = iC;
                else{
                    check(Nodes[C->Parent].Child2 == iA);
                    Nodes[C->Parent].Child2 = iC;
                }
            }
            else
                Root = iC;

            if(F->Height > G->Height){
                C->Child2 = iF;
                A->Child2 = iG;
                G->Parent = iA;
                A->Bound.Combine(B->Bound, G->Bound);
                C->Bound.Combine(A->Bound, F->Bound);

                A->Height = 1 + FMath::Max(B->Height, G->Height);
                C->Height = 1 + FMath::Max(A->Height, F->Height);
            }
            else{
                C->Child2 = iG;
                A->Child2 = iF;
                F->Parent = iA;
                A->Bound.Combine(B->Bound, F->Bound);
                C->Bound.Combine(A->Bound, G->Bound);

                A->Height = 1 + FMath::Max(B->Height, F->Height);
                C->Height = 1 + FMath::Max(A->Height, G->Height);
            }

            return iC;
        }

        if(Balance < -1){
            SizeType iD = B->Child1;
            SizeType iE = B->Child2;
            check(0 <= iD && iD < NodeCapacity);
            check(0 <= iE && iE < NodeCapacity);

            NodeType* D = Nodes + iD;
            NodeType* E = Nodes + iE;

            B->Child1 = iA;
            B->Parent = A->Parent;
            A->Parent = iB;

            if(B->Parent != __hidden_DynamicBVH::NodeNull<SizeType>){
                if(Nodes[B->Parent].Child1 == iA)
                    Nodes[B->Parent].Child1 = iB;
                else{
                    check(Nodes[B->Parent].Child2 == iA);
                    Nodes[B->Parent].Child2 = iB;
                }
            }
            else
                Root = iB;

            if(D->Height > E->Height){
                B->Child2 = iD;
                A->Child1 = iE;
                E->Parent = iA;
                A->Bound.Combine(C->Bound, E->Bound);
                B->Bound.Combine(A->Bound, D->Bound);

                A->Height = 1 + FMath::Max(C->Height, E->Height);
                B->Height = 1 + FMath::Max(A->Height, D->Height);
            }
            else{
                B->Child2 = iE;
                A->Child1 = iD;
                D->Parent = iA;
                A->Bound.Combine(C->Bound, D->Bound);
                B->Bound.Combine(A->Bound, E->Bound);

                A->Height = 1 + FMath::Max(C->Height, D->Height);
                B->Height = 1 + FMath::Max(A->Height, E->Height);
            }

            return iB;
        }

        return iA;
    }


private:
    void ValidateStructure(SizeType Index)const{
        if(Index == __hidden_DynamicBVH::NodeNull<SizeType>)
            return;

        if(Index == Root)
            check(Nodes[Index].Parent == __hidden_DynamicBVH::NodeNull<SizeType>);

        const NodeType* Node = Nodes + Index;

        SizeType Child1 = Node->Child1;
        SizeType Child2 = Node->Child2;

        if(Node->IsLeaf()){
            check(Child1 == __hidden_DynamicBVH::NodeNull<SizeType>);
            check(Child2 == __hidden_DynamicBVH::NodeNull<SizeType>);
            check(Node->Height == 0);
            return;
        }

        check(0 <= Child1 && Child1 << NodeCapacity);
        check(0 <= Child2 && Child2 << NodeCapacity);

        check(Nodes[Child1].Parent == Index);
        check(Nodes[Child2].Parent == Index);

        ValidateStructure(Child1);
        ValidateStructure(Child2);
    }
    void ValidateMetrics(SizeType Index)const{
        if(Index == __hidden_DynamicBVH::NodeNull<SizeType>)
            return;

        const NodeType* Node = Nodes + Index;

        SizeType Child1 = Node->Child1;
        SizeType Child2 = Node->Child2;

        if(Node->IsLeaf()){
            check(Child1 == __hidden_DynamicBVH::NodeNull<SizeType>);
            check(Child2 == __hidden_DynamicBVH::NodeNull<SizeType>);
            check(Node->Height == 0);
            return;
        }

        check(0 <= Child1 && Child1 << NodeCapacity);
        check(0 <= Child2 && Child2 << NodeCapacity);

        SizeType Height1 = Nodes[Child1].Height;
        SizeType Height2 = Nodes[Child2].Height;
        SizeType Height;
        Height = 1 + FMath::Max(Height1, Height2);
        check(Node->Height == Height);

        BoundType Bound;
        Bound.Combine(Nodes[Child1].Bound, Nodes[Child2].Bound);

        // ensure(Bound.Lower == Node->Bound.Lower);
        // ensure(Bound.Upper == Node->Bound.Upper);

        ValidateMetrics(Child1);
        ValidateMetrics(Child2);
    }

private:
    SizeType ComputeHeight()const{
        SizeType Height = ComputeHeight(Root);
        return Height;
    }
    SizeType ComputeHeight(SizeType NodeID)const{
        check(0 <= NodeID && NodeID < NodeCapacity);
        NodeType* Node = Nodes + NodeID;

        if(Node->IsLeaf())
            return 0;

        SizeType Height1 = ComputeHeight(Node->Child1);
        SizeType Height2 = ComputeHeight(Node->Child2);
        return 1 + FMath::Max(Height1, Height2);
    }


private:
    DataContainer Data;

private:
    NodeType* Nodes;

    SizeType Root;

    SizeType NodeCount;
    SizeType NodeCapacity;

    SizeType FreeList;

    SizeType InsertionCount;
};


template<typename InElementType, typename InAllocator = TBVHAllocator, typename BoundType = TBVHBound2D<typename InAllocator::FloatType>, int32 QueryStackCapacity = 1 << 11>
using TDynamicBVH2D = TDynamicBVH<InElementType, BoundType, InAllocator, QueryStackCapacity>; 


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

