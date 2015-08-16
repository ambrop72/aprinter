/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef APRINTER_BED_PROBE_H
#define APRINTER_BED_PROBE_H

#include <stdint.h>
#include <math.h>

#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/StructIf.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/WrapValue.h>
#include <aprinter/meta/IndexElemList.h>
#include <aprinter/meta/JoinTypeLists.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/math/Matrix.h>
#include <aprinter/math/LinearLeastSquares.h>
#include <aprinter/printer/Configuration.h>

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class BedProbe {
public:
    struct Object;
    
private:
    using ProbePoints = typename Params::ProbePoints;
    using PlatformAxesList = typename Params::PlatformAxesList;
    using CorrectionParams = typename Params::ProbeCorrectionParams;
    static const int NumPoints = TypeListLength<ProbePoints>::Value;
    static const int NumPlatformAxes = TypeListLength<PlatformAxesList>::Value;
    
    using Config = typename ThePrinterMain::Config;
    using TheCommand = typename ThePrinterMain::TheCommand;
    using FpType = typename ThePrinterMain::FpType;
    static const int ProbeAxisIndex = ThePrinterMain::template FindPhysVirtAxis<Params::ProbeAxis>::Value;
    
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_print_correction, print_correction)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_fill_point_coordinates, fill_point_coordinates)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_calc_correction_contribution, calc_correction_contribution)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_correct_virt_axis, correct_virt_axis)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_skip_point_if_disabled, skip_point_if_disabled)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_get_z_offset, get_z_offset)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_get_coord, get_coord)
    AMBRO_DECLARE_LIST_FOREACH_HELPER(LForeach_add_axis, add_axis)
    
public:
    static bool const CorrectionSupported = CorrectionParams::Enabled;
    
    AMBRO_STRUCT_IF(CorrectionFeature, CorrectionSupported) {
        friend BedProbe;
        static_assert(ThePrinterMain::IsTransformEnabled, "");
        static_assert(ThePrinterMain::template IsVirtAxis<ProbeAxisIndex>::Value, "");
        
    private:
        static void init (Context c)
        {
            auto *o = Object::self(c);
            MatrixWriteZero(o->corrections--);
        }
        
        static void apply_corrections (Context c)
        {
            ThePrinterMain::TransformFeature::handle_corrections_change(c);
        }
        
        static void print_corrections (Context c, TheCommand *cmd, Matrix<FpType, NumPlatformAxes + 1, 1> const *corrections, AMBRO_PGM_P msg)
        {
            cmd->reply_append_pstr(c, msg);
            ListForEachForward<AxisHelperList>(LForeach_print_correction(), c, cmd, corrections);
            cmd->reply_append_ch(c, ' ');
            cmd->reply_append_ch(c, ThePrinterMain::template PhysVirtAxisHelper<ProbeAxisIndex>::AxisName);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_fp(c, (*corrections)++(NumPlatformAxes, 0));
            cmd->reply_append_ch(c, '\n');
        }
        
        static bool check_command (Context c, TheCommand *cmd)
        {
            auto *o = Object::self(c);
            if (cmd->getCmdNumber(c) == 937) {
                print_corrections(c, cmd, &o->corrections, AMBRO_PSTR("EffectiveCorrections"));
                cmd->finishCommand(c);
                return false;
            }
            if (cmd->getCmdNumber(c) == 938) {
                if (!cmd->tryUnplannedCommand(c)) {
                    return false;
                }
                MatrixWriteZero(o->corrections--);
                apply_corrections(c);
                cmd->finishCommand(c);
                return false;
            }
            return true;
        }
        
        static void probing_staring (Context c)
        {
            auto *o = Object::self(c);
            for (int i = 0; i < NumPoints; i++) {
                o->heights_matrix--(i, 0) = NAN;
            }
        }
        
        static void probing_measurement (Context c, uint8_t point_index, FpType height)
        {
            auto *o = Object::self(c);
            o->heights_matrix--(point_index, 0) = height;
        }
        
        static void probing_completing (Context c, TheCommand *cmd)
        {
            auto *o = Object::self(c);
            
            Matrix<FpType, NumPoints, NumPlatformAxes + 1> coordinates_matrix;
            
            ListForEachForward<AxisHelperList>(LForeach_fill_point_coordinates(), c, coordinates_matrix--);
            
            int num_valid_points = 0;
            for (int i = 0; i < NumPoints; i++) {
                if (isnan(o->heights_matrix--(i, 0))) {
                    continue;
                }
                coordinates_matrix--(i, NumPlatformAxes) = 1.0f;
                if (i != num_valid_points) {
                    MatrixCopy(coordinates_matrix--.range(num_valid_points, 0, 1, NumPlatformAxes + 1), coordinates_matrix++.range(i, 0, 1, NumPlatformAxes + 1));
                    MatrixCopy(o->heights_matrix--.range(num_valid_points, 0, 1, 1), o->heights_matrix++.range(i, 0, 1, 1));
                }
                num_valid_points++;
            }
            
            if (num_valid_points < NumPlatformAxes + 1) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:TooFewPointsForCorrection\n"));
                return;
            }
            
            auto effective_coordinates_matrix = coordinates_matrix--.range(0, 0, num_valid_points, NumPlatformAxes + 1);
            auto effective_heights_matrix = o->heights_matrix--.range(0, 0, num_valid_points, 1);
            
            Matrix<FpType, NumPlatformAxes + 1, 1> new_corrections;
            
            LinearLeastSquaresMaxSize<NumPoints, NumPlatformAxes + 1>(effective_coordinates_matrix--, effective_heights_matrix++, new_corrections--);
            
            print_corrections(c, cmd, &new_corrections, AMBRO_PSTR("RelativeCorrections"));
            
            bool bad_corrections = false;
            for (int i = 0; i < NumPlatformAxes + 1; i++) {
                FpType x = new_corrections++(i, 0);
                if (isnan(x) || isinf(x)) {
                    bad_corrections = true;
                }
            }
            
            if (bad_corrections) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:BadCorrections\n"));
                return;
            }
            
            if (cmd->find_command_param(c, 'A', nullptr)) {
                MatrixElemOpInPlace<MatrixElemOpAdd>(o->corrections--, new_corrections++);
                apply_corrections(c);
            }
        }
        
        template <typename Src>
        static FpType compute_correction_for_point (Context c, Src src)
        {
            auto *o = Object::self(c);
            FpType constant_correction = o->corrections++(NumPlatformAxes, 0);
            FpType linear_correction = ListForEachForwardAccRes<AxisHelperList>(0.0f, LForeach_calc_correction_contribution(), c, src, &o->corrections);
            return constant_correction + linear_correction;
        }
        
        template <int VirtAxisIndex>
        struct VirtAxisHelper {
            template <typename Src, typename Dst, bool Reverse>
            static void correct_virt_axis (Context c, Src src, Dst dst, FpType correction_value, WrapBool<Reverse>)
            {
                FpType coord_value = src.template get<VirtAxisIndex>();
                if (VirtAxisIndex == ThePrinterMain::template GetVirtAxisVirtIndex<ProbeAxisIndex>::Value) {
                    if (Reverse) {
                        coord_value -= correction_value;
                    } else {
                        coord_value += correction_value;
                    }
                }
                dst.template set<VirtAxisIndex>(coord_value);
            }
        };
        using VirtAxisHelperList = IndexElemListCount<ThePrinterMain::TransformFeature::NumVirtAxes, VirtAxisHelper>;
        
    public:
        static bool const CorrectionEnabled = true;
        
        template <typename Src, typename Dst, bool Reverse>
        static void do_correction (Context c, Src src, Dst dst, WrapBool<Reverse>)
        {
            FpType correction_value = compute_correction_for_point(c, src);
            ListForEachForward<VirtAxisHelperList>(LForeach_correct_virt_axis(), c, src, dst, correction_value, WrapBool<Reverse>());
        }
        
    public:
        struct Object : public ObjBase<CorrectionFeature, typename BedProbe::Object, EmptyTypeList> {
            Matrix<FpType, NumPoints, 1> heights_matrix;
            Matrix<FpType, NumPlatformAxes + 1, 1> corrections;
        };
    } AMBRO_STRUCT_ELSE(CorrectionFeature) {
        static void init (Context c) {}
        static bool check_command (Context c, TheCommand *cmd) { return true; }
        static void probing_staring (Context c) {}
        static void probing_measurement (Context c, uint8_t point_index, FpType height) {}
        static void probing_completing (Context c, TheCommand *cmd) {}
        struct Object {};
    };
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        o->m_current_point = 0xff;
        Context::Pins::template setInput<typename Params::ProbePin, typename Params::ProbePinInputMode>(c);
        CorrectionFeature::init(c);
    }
    
    static void deinit (Context c)
    {
    }
    
    static bool check_command (Context c, TheCommand *cmd)
    {
        auto *o = Object::self(c);
        if (cmd->getCmdNumber(c) == 32) {
            if (!cmd->tryUnplannedCommand(c)) {
                return false;
            }
            AMBRO_ASSERT(o->m_current_point == 0xff)
            o->m_current_point = 0;
            skip_disabled_points_and_detect_end(c);
            if (o->m_current_point == 0xff) {
                cmd->reply_append_pstr(c, AMBRO_PSTR("Error:NoProbePointsEnabled\n"));
                cmd->finishCommand(c);
            } else {
                init_probe_planner(c, false);
                o->m_point_state = 0;
                o->m_command_sent = false;
                CorrectionFeature::probing_staring(c);
            }
            return false;
        }
        return CorrectionFeature::check_command(c, cmd);
    }
    
    static void m119_append_endstop (Context c, TheCommand *cmd)
    {
        bool triggered = endstop_is_triggered(c);
        cmd->reply_append_pstr(c, AMBRO_PSTR(" Probe:"));
        cmd->reply_append_ch(c, (triggered ? '1' : '0'));
    }
    
    template <typename CallbackContext>
    static bool prestep_callback (CallbackContext c)
    {
        return endstop_is_triggered(c);
    }
    
private:
    template <typename ThisContext>
    static bool endstop_is_triggered (ThisContext c)
    {
        return Context::Pins::template get<typename Params::ProbePin>(c) != APRINTER_CFG(Config, CProbeInvert, c);
    }
    
    template <int PointIndex>
    struct PointHelper {
        using Point = TypeListGet<ProbePoints, PointIndex>;
        
        using CPointEnabled = decltype(ExprCast<bool>(Config::e(Point::Enabled::i())));
        using CPointZOffset = decltype(ExprCast<FpType>(Config::e(Point::ZOffset::i())));
        
        template <int PlatformAxisIndex>
        using CPointCoordForAxis = decltype(ExprCast<FpType>(Config::e(TypeListGet<typename Point::Coords, PlatformAxisIndex>::i())));
        using CPointCoordList = IndexElemListCount<NumPlatformAxes, CPointCoordForAxis>;
        
        using ConfigExprs = JoinTypeLists<MakeTypeList<CPointEnabled, CPointZOffset>, CPointCoordList>;
        
        static uint8_t skip_point_if_disabled (uint8_t point_index, Context c)
        {
            if (point_index == PointIndex && !APRINTER_CFG(Config, CPointEnabled, c)) {
                point_index++;
            }
            return point_index;
        }
        
        static FpType get_z_offset (Context c)
        {
            return APRINTER_CFG(Config, CPointZOffset, c);
        }
        
        template <int PlatformAxisIndex>
        static FpType get_coord (Context c, WrapInt<PlatformAxisIndex>)
        {
            using CPointCoord = TypeListGet<CPointCoordList, PlatformAxisIndex>;
            return APRINTER_CFG(Config, CPointCoord, c);
        }
        
        struct Object : public ObjBase<PointHelper, typename BedProbe::Object, EmptyTypeList> {};
    };
    using PointHelperList = IndexElemList<ProbePoints, PointHelper>;
    
    static FpType get_point_z_offset (Context c, uint8_t point_index)
    {
        return ListForOneOffset<PointHelperList, 0, FpType>(point_index, LForeach_get_z_offset(), c);
    }
    
    template <int PlatformAxisIndex>
    static FpType get_point_coord (Context c, uint8_t point_index)
    {
        return ListForOneOffset<PointHelperList, 0, FpType>(point_index, LForeach_get_coord(), c, WrapInt<PlatformAxisIndex>());
    }
    
    static void skip_disabled_points_and_detect_end (Context c)
    {
        auto *o = Object::self(c);
        o->m_current_point = ListForEachForwardAccRes<PointHelperList>(o->m_current_point, LForeach_skip_point_if_disabled(), c);
        if (o->m_current_point >= NumPoints) {
            o->m_current_point = 0xff;
        }
    }
    
    template <int PlatformAxisIndex>
    struct AxisHelper {
        struct Object;
        using PlatformAxis = TypeListGet<PlatformAxesList, PlatformAxisIndex>;
        static const int AxisIndex = ThePrinterMain::template FindPhysVirtAxis<PlatformAxis::Value>::Value;
        using AxisProbeOffset = TypeListGet<typename Params::ProbePlatformOffset, PlatformAxisIndex>;
        
        using CAxisProbeOffset = decltype(ExprCast<FpType>(Config::e(AxisProbeOffset::i())));
        using ConfigExprs = MakeTypeList<CAxisProbeOffset>;
        
        static void add_axis (Context c, uint8_t point_index)
        {
            FpType coord = get_point_coord<PlatformAxisIndex>(c, point_index);
            ThePrinterMain::template move_add_axis<AxisIndex>(c, coord + APRINTER_CFG(Config, CAxisProbeOffset, c));
        }
        
        static void fill_point_coordinates (Context c, MatrixRange<FpType> matrix)
        {
            for (int i = 0; i < NumPoints; i++) {
                matrix(i, PlatformAxisIndex) = get_point_coord<PlatformAxisIndex>(c, i);
            }
        }
        
        template <typename TheCorrectionFeature=CorrectionFeature>
        static void print_correction (Context c, TheCommand *cmd, Matrix<FpType, NumPlatformAxes + 1, 1> const *corrections)
        {
            cmd->reply_append_ch(c, ' ');
            cmd->reply_append_ch(c, ThePrinterMain::template PhysVirtAxisHelper<AxisIndex>::AxisName);
            cmd->reply_append_ch(c, ':');
            cmd->reply_append_fp(c, (*corrections)++(PlatformAxisIndex, 0));
        }
        
        template <typename Src>
        static FpType calc_correction_contribution (FpType accum, Context c, Src src, Matrix<FpType, NumPlatformAxes + 1, 1> const *corrections)
        {
            static_assert(ThePrinterMain::template IsVirtAxis<AxisIndex>::Value, "");
            static int const VirtAxisIndex = ThePrinterMain::template GetVirtAxisVirtIndex<AxisIndex>::Value;
            return accum + src.template get<VirtAxisIndex>() * (*corrections)++(PlatformAxisIndex, 0);
        }
        
        struct Object : public ObjBase<AxisHelper, typename BedProbe::Object, EmptyTypeList> {};
    };
    using AxisHelperList = IndexElemList<PlatformAxesList, AxisHelper>;
    
    class ProbePlannerClient : public ThePrinterMain::PlannerClient {
    public:
        void pull_handler (Context c)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_current_point != 0xff)
            AMBRO_ASSERT(o->m_point_state <= 4)
            
            if (o->m_command_sent) {
                ThePrinterMain::custom_planner_wait_finished(c);
                return;
            }
            
            ThePrinterMain::move_begin(c);
            
            FpType height;
            FpType time_freq_by_speed;
            switch (o->m_point_state) {
                case 0: {
                    ListForEachForward<AxisHelperList>(LForeach_add_axis(), c, o->m_current_point);
                    height = APRINTER_CFG(Config, CProbeStartHeight, c);
                    time_freq_by_speed = APRINTER_CFG(Config, CProbeMoveSpeedFactor, c);
                } break;
                case 1: {
                    height = APRINTER_CFG(Config, CProbeLowHeight, c);
                    time_freq_by_speed = APRINTER_CFG(Config, CProbeFastSpeedFactor, c);
                } break;
                case 2: {
                    height = get_height(c) + APRINTER_CFG(Config, CProbeRetractDist, c);
                    time_freq_by_speed = APRINTER_CFG(Config, CProbeRetractSpeedFactor, c);
                } break;
                case 3: {
                    height = APRINTER_CFG(Config, CProbeLowHeight, c);
                    time_freq_by_speed = APRINTER_CFG(Config, CProbeSlowSpeedFactor, c);
                } break;
                case 4: {
                    height = APRINTER_CFG(Config, CProbeStartHeight, c);
                    time_freq_by_speed = APRINTER_CFG(Config, CProbeRetractSpeedFactor, c);
                } break;
            }
            
            ThePrinterMain::template move_add_axis<ProbeAxisIndex>(c, height, true);
            ThePrinterMain::move_end(c, time_freq_by_speed);
            
            o->m_command_sent = true;
        }
        
        void finished_handler (Context c)
        {
            finished_or_aborted(c, false);
        }
        
        void aborted_handler (Context c)
        {
            finished_or_aborted(c, true);
        }
        
    private:
        void finished_or_aborted (Context c, bool aborted)
        {
            auto *o = Object::self(c);
            AMBRO_ASSERT(o->m_current_point != 0xff)
            AMBRO_ASSERT(o->m_point_state <= 4)
            AMBRO_ASSERT(o->m_command_sent)
            AMBRO_ASSERT(!aborted || is_point_state_watching(o->m_point_state))
            
            ThePrinterMain::custom_planner_deinit(c);
            o->m_command_sent = false;
            
            if (is_point_state_watching(o->m_point_state) && !aborted) {
                return finish_probing(c, true, AMBRO_PSTR("Error:EndstopNotTriggeredInProbeMove\n"));
            }
            
            if (o->m_point_state == 4) {
                o->m_current_point++;
                skip_disabled_points_and_detect_end(c);
                if (o->m_current_point == 0xff) {
                    return finish_probing(c, false, nullptr);
                }
                init_probe_planner(c, false);
                o->m_point_state = 0;
                return;
            }
            
            if (o->m_point_state == 3) {
                FpType height = get_height(c) + APRINTER_CFG(Config, CProbeGeneralZOffset, c) + get_point_z_offset(c, o->m_current_point);
                report_height(c, ThePrinterMain::get_locked(c), o->m_current_point, height);
            }
            
            o->m_point_state++;
            bool watch_probe = is_point_state_watching(o->m_point_state);
            
            if (watch_probe && endstop_is_triggered(c)) {
                return finish_probing(c, true, AMBRO_PSTR("Error:EndstopTriggeredBeforeProbeMove\n"));
            }
            
            init_probe_planner(c, watch_probe);
        }
    };
    
    static void init_probe_planner (Context c, bool watch_probe)
    {
        auto *o = Object::self(c);
        ThePrinterMain::custom_planner_init(c, &o->planner_client, watch_probe);
    }
    
    static FpType get_height (Context c)
    {
        return ThePrinterMain::template PhysVirtAxisHelper<ProbeAxisIndex>::get_position(c);
    }
    
    static void report_height (Context c, TheCommand *cmd, uint8_t point_index, FpType height)
    {
        CorrectionFeature::probing_measurement(c, point_index, height);
        
        cmd->reply_append_pstr(c, AMBRO_PSTR("//ProbeHeight@P"));
        cmd->reply_append_uint16(c, point_index + 1);
        cmd->reply_append_ch(c, ' ');
        cmd->reply_append_fp(c, height);
        cmd->reply_append_ch(c, '\n');
        cmd->reply_poke(c);
    }
    
    static void finish_probing (Context c, bool error, AMBRO_PGM_P errstr)
    {
        auto *o = Object::self(c);
        
        o->m_current_point = 0xff;
        
        TheCommand *cmd = ThePrinterMain::get_locked(c);
        if (error) {
            cmd->reply_append_pstr(c, errstr);
        } else {
            CorrectionFeature::probing_completing(c, cmd);
        }
        cmd->finishCommand(c);
    }
    
    static bool is_point_state_watching (uint8_t point_state)
    {
        return point_state == 1 || point_state == 3;
    }
    
    using CProbeInvert = decltype(ExprCast<bool>(Config::e(Params::ProbeInvert::i())));
    using CProbeStartHeight = decltype(ExprCast<FpType>(Config::e(Params::ProbeStartHeight::i())));
    using CProbeLowHeight = decltype(ExprCast<FpType>(Config::e(Params::ProbeLowHeight::i())));
    using CProbeRetractDist = decltype(ExprCast<FpType>(Config::e(Params::ProbeRetractDist::i())));
    using CProbeMoveSpeedFactor = decltype(ExprCast<FpType>(typename ThePrinterMain::TimeConversion() / Config::e(Params::ProbeMoveSpeed::i())));
    using CProbeFastSpeedFactor = decltype(ExprCast<FpType>(typename ThePrinterMain::TimeConversion() / Config::e(Params::ProbeFastSpeed::i())));
    using CProbeRetractSpeedFactor = decltype(ExprCast<FpType>(typename ThePrinterMain::TimeConversion() / Config::e(Params::ProbeRetractSpeed::i())));
    using CProbeSlowSpeedFactor = decltype(ExprCast<FpType>(typename ThePrinterMain::TimeConversion() / Config::e(Params::ProbeSlowSpeed::i())));
    using CProbeGeneralZOffset = decltype(ExprCast<FpType>(Config::e(Params::ProbeGeneralZOffset::i())));
    
public:
    using ConfigExprs = MakeTypeList<
        CProbeInvert, CProbeStartHeight, CProbeLowHeight, CProbeRetractDist, CProbeMoveSpeedFactor,
        CProbeFastSpeedFactor, CProbeRetractSpeedFactor, CProbeSlowSpeedFactor, CProbeGeneralZOffset
    >;
    
public:
    struct Object : public ObjBase<BedProbe, ParentObject, JoinTypeLists<
        PointHelperList,
        AxisHelperList,
        MakeTypeList<CorrectionFeature>
    >> {
        ProbePlannerClient planner_client;
        uint8_t m_current_point;
        uint8_t m_point_state;
        bool m_command_sent;
    };
};

struct BedProbeNoCorrectionParams {
    static bool const Enabled = false;
};

struct BedProbeCorrectionParams {
    static bool const Enabled = true;
};

template <
    typename TEnabled,
    typename TCoords,
    typename TZOffset
>
struct BedProbePointParams {
    using Enabled = TEnabled;
    using Coords = TCoords;
    using ZOffset = TZOffset;
};

template <
    typename TPlatformAxesList,
    char TProbeAxis,
    typename TProbePin,
    typename TProbePinInputMode,
    typename TProbeInvert,
    typename TProbePlatformOffset,
    typename TProbeStartHeight,
    typename TProbeLowHeight,
    typename TProbeRetractDist,
    typename TProbeMoveSpeed,
    typename TProbeFastSpeed,
    typename TProbeRetractSpeed,
    typename TProbeSlowSpeed,
    typename TProbeGeneralZOffset,
    typename TProbePoints,
    typename TProbeCorrectionParams
>
struct BedProbeService {
    using PlatformAxesList = TPlatformAxesList;
    static char const ProbeAxis = TProbeAxis;
    using ProbePin = TProbePin;
    using ProbePinInputMode = TProbePinInputMode;
    using ProbeInvert = TProbeInvert;
    using ProbePlatformOffset = TProbePlatformOffset;
    using ProbeStartHeight = TProbeStartHeight;
    using ProbeLowHeight = TProbeLowHeight;
    using ProbeRetractDist = TProbeRetractDist;
    using ProbeMoveSpeed = TProbeMoveSpeed;
    using ProbeFastSpeed = TProbeFastSpeed;
    using ProbeRetractSpeed = TProbeRetractSpeed;
    using ProbeSlowSpeed = TProbeSlowSpeed;
    using ProbeGeneralZOffset = TProbeGeneralZOffset;
    using ProbePoints = TProbePoints;
    using ProbeCorrectionParams = TProbeCorrectionParams;
    
    template <typename Context, typename ParentObject, typename ThePrinterMain>
    using Probe = BedProbe<Context, ParentObject, ThePrinterMain, BedProbeService>;
};

#include <aprinter/EndNamespace.h>

#endif
