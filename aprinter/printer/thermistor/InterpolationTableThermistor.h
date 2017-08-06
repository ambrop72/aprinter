/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APRINTER_INTERPOLATION_TABLE_THERMISTOR_H
#define APRINTER_INTERPOLATION_TABLE_THERMISTOR_H

#include <stddef.h>
#include <math.h>

#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/StaticArray.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/Expr.h>
#include <aprinter/meta/ChooseInt.h>
#include <aprinter/base/Preprocessor.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Hints.h>
#include <aprinter/math/FloatTools.h>

namespace APrinter {

template <typename Arg>
class InterpolationTableThermistor {
    APRINTER_USE_TYPES1(Arg, (Context, FpType, Params))
    APRINTER_USE_TYPES1(Params, (TableDef))
    
    static int const TableLength = APRINTER_ARRAY_LEN(TableDef::Table);
    static_assert(TableLength >= 2, "");
    
    // Integer type sufficient to point to a table entry
    using IndexType = ChooseIntForMax<TableLength, false>;
    
    // Floating point type for table entries (ADC and temperature)
    using TableFpType = float;
    
public:
    // Whether the conversion has positive or negative slope
    static bool const NegativeSlope = TableDef::NegativeSlope;
    
    // Convert ADC value to temperature
    static FpType adcToTemp (Context, FpType adc)
    {
        if (AMBRO_UNLIKELY(FloatIsNan(adc))) {
            return adc;
        }
        
        IndexType i = 0;
        IndexType j = TableLength - 1;
        
        FpType adc_i = AdcArray::readAt(i);
        FpType adc_j = AdcArray::readAt(j);
        
        if (AMBRO_UNLIKELY(!(adc >= adc_i))) {
            return NegativeSlope ? INFINITY : -INFINITY;
        }
        if (AMBRO_UNLIKELY(!(adc <= adc_j))) {
            return NegativeSlope ? -INFINITY : INFINITY;
        }
        
        while (j - i > 1) {
            IndexType k = (i + j) / 2;
            FpType adc_k = AdcArray::readAt(k);
            
            if (adc < adc_k) {
                j = k;
                adc_j = adc_k;
            } else {
                i = k;
                adc_i = adc_k;
            }
        }
        
        AMBRO_ASSERT(j == i + 1)
        
        FpType temp_i = TempArray::readAt(i);
        FpType temp_j = TempArray::readAt(j);
        
        return interpolate(adc, adc_i, adc_j, temp_i, temp_j);
    }

private:
    static constexpr FpType interpolate (FpType x, FpType x1, FpType x2, FpType y1, FpType y2)
    {
        FpType frac = (x - x1) / (x2 - x1);
        return ((FpType)1 - frac) * y1 + frac * y2;
    }
    
    // This is given to StaticArray to get the value of each AdcArray element.
    template <int EntryIndex>
    class GetAdcArrayEntry {
        // Retrieve the entry from the constexpr array within a static
        // constexpr variable to ensure the original does not have linkage.
        // Divide the input by InputDivide so it doesn't need to be
        // done at runtime.
        static constexpr double StaticValue =
            TableDef::Table[EntryIndex].input / TableDef::InputDivide;
        
    public:
        // This is called from StaticArray to get an entry.
        static constexpr TableFpType value ()
        {
            return StaticValue;
        }
    };
    
    // This is given to StaticArray to get the value of each TempArray element.
    template <int EntryIndex>
    class GetTempArrayEntry {
        // See above why this needs to be assigned like this.
        static constexpr double StaticValue = TableDef::Table[EntryIndex].temp;
        
    public:
        // This is called from StaticArray to get an entry.
        static constexpr TableFpType value ()
        {
            return StaticValue;
        }
    };
    
    // Program memory arrays (one for inputs and another for temperatures)
    using AdcArray = StaticArray<TableFpType, TableLength, GetAdcArrayEntry>;
    using TempArray = StaticArray<TableFpType, TableLength, GetTempArrayEntry>;
    
    // TODO: There might be a way to use the same code for compile-time
    // and runtime tempToAdc. The problem is with AVR where we want to
    // absolutely avoid any accidental overhead from compile-time access
    // of the lookup table, such as a non-progmem definition of the lookup
    // table that goes to RAM.
    
    // This implements compile-time mapping from temparature to ADC values.
    // The initial invocation is done by StaticTempToAdcStart.
    template <int IndexI, int IndexJ, typename Temp>
    struct StaticTempToAdcBisect {
        static_assert(IndexI >= 0 && IndexJ < TableLength, "");
        static_assert(IndexJ > IndexI, "");
        
        static constexpr double TempI = TempArray::template ReadAt<IndexI>::value();
        static constexpr double TempJ = TempArray::template ReadAt<IndexJ>::value();
        
        AMBRO_STRUCT_IF(CheckEnd, IndexJ == IndexI + 1) {
            static constexpr double AdcI = AdcArray::template ReadAt<IndexI>::value();
            static constexpr double AdcJ = AdcArray::template ReadAt<IndexJ>::value();
            
            using Result = AMBRO_WRAP_DOUBLE(
                interpolate(Temp::value(), TempI, TempJ, AdcI, AdcJ));
        }
        AMBRO_STRUCT_ELSE(CheckEnd) {
            static int const IndexK = (IndexI + IndexJ) / 2;
            static constexpr double TempK = TempArray::template ReadAt<IndexK>::value();
            static bool const LeftOfK = (Temp::value() < TempK) != NegativeSlope;
            
            using Result = typename StaticTempToAdcBisect<
                (LeftOfK ? IndexI : IndexK),
                (LeftOfK ? IndexK : IndexJ),
                Temp
            >::Result;
        };
        
        using Result = typename CheckEnd::Result;
    };
    
    // This starts the conversion by checking the temperature range and
    // setting initial indices.
    template <typename Temp>
    struct StaticTempToAdcStart {
        // Initial indices for bisection.
        static int const IndexI = 0;
        static int const IndexJ = TableLength - 1;
        
        static constexpr double TempI = TempArray::template ReadAt<IndexI>::value();
        static constexpr double TempJ = TempArray::template ReadAt<IndexJ>::value();
        
        // Sanity check temperature range.
        static_assert(Temp::value() >= (NegativeSlope ? TempJ : TempI), "");
        static_assert(Temp::value() <= (NegativeSlope ? TempI : TempJ), "");
        
        // Do the bisection and interpolation.
        using Result = typename StaticTempToAdcBisect<IndexI, IndexJ, Temp>::Result;
    };
    
    // And almost exactly the same thing for runtime use. Fun!
    static FpType tempToAdc (FpType temp)
    {
        if (AMBRO_UNLIKELY(FloatIsNan(temp))) {
            return temp;
        }
        
        IndexType i = 0;
        IndexType j = TableLength - 1;
        
        FpType temp_i = TempArray::readAt(i);
        FpType temp_j = TempArray::readAt(j);
        
        // Since this is used to determine temperature safe limits, it is
        // appropriate to return boundary ADC values if the temperature
        // is out of range, rather than infinities.
        if (AMBRO_UNLIKELY(!(temp >= (NegativeSlope ? temp_j : temp_i)))) {
            return AdcArray::readAt(NegativeSlope ? j : i);
        }
        if (AMBRO_UNLIKELY(!(temp <= (NegativeSlope ? temp_i : temp_j)))) {
            return AdcArray::readAt(NegativeSlope ? i : j);
        }
        
        while (j - i > 1) {
            IndexType k = (i + j) / 2;
            FpType temp_k = TempArray::readAt(k);
            
            if ((temp < temp_k) != NegativeSlope) {
                j = k;
                temp_j = temp_k;
            } else {
                i = k;
                temp_i = temp_k;
            }
        }
        
        AMBRO_ASSERT(j == i + 1)
        
        FpType adc_i = AdcArray::readAt(i);
        FpType adc_j = AdcArray::readAt(j);
        
        return interpolate(temp, temp_i, temp_j, adc_i, adc_j);
    }
    
    // Define Expr function class which uses StaticTempToAdcStart for
    // compile-time and tempToAdc for runtime evaluation. The resulting
    // class is really called ExprFunc__TempToAdc.
    APRINTER_DEFINE_UNARY_EXPR_FUNC_CLASS(TempToAdc,
        StaticTempToAdcStart<Op1>::Result::value(),
        tempToAdc(arg1)
    )
    
public:
    // Expr function to convert temperature to ADC value,
    // which supports compile-time and run-time evaluation.
    template <typename Temp>
    static auto TempToAdc (Temp) -> NaryExpr<ExprFunc__TempToAdc, Temp>;
    
public:
    struct Object {};
};

// This is used only at compile time for lookup table entries
struct InterpolationTableEntry {
    double input;
    double temp;
};

// This macro is used to define conversions based on lookup tables.
// The result is a class that contains the lookup table entries
// and related parameters, all as constexpr variables.
#define APRINTER_DEFINE_INTERPOLATION_TABLE(TableName, HasNegativeSlope, InputDivideVal, TableEntries) \
struct TableName { \
    static bool const NegativeSlope = HasNegativeSlope; \
    static constexpr double InputDivide = InputDivideVal; \
    static constexpr InterpolationTableEntry Table[] = APRINTER_REMOVE_PARENS TableEntries; \
};

APRINTER_ALIAS_STRUCT_EXT(InterpolationTableThermistorService, (
    APRINTER_AS_TYPE(TableDef)
), (
    APRINTER_ALIAS_STRUCT_EXT(Formula, (
        APRINTER_AS_TYPE(Context),
        APRINTER_AS_TYPE(ParentObject),
        APRINTER_AS_TYPE(Config),
        APRINTER_AS_TYPE(FpType)
    ), (
        using Params = InterpolationTableThermistorService;
        APRINTER_DEF_INSTANCE(Formula, InterpolationTableThermistor)
    ))
))

}

#endif
