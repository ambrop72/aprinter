# Copyright (c) 2015 Ambroz Bizjak
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function
import sys
import os
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '../utils'))
import argparse
import json
import re
import string
import config_reader
import selection
import function_defined
import assign_func
import rich_template
import file_utils
import nix_utils

IDENTIFIER_REGEX = '\\A[A-Za-z][A-Za-z0-9_]{0,127}\\Z'

class GenState(object):
    def __init__ (self):
        self._subst = {}
        self._config_options = []
        self._constants = []
        self._platform_includes = []
        self._aprinter_includes = set()
        self._objects = {}
        self._singleton_objects = {}
        self._finalize_actions = []
        self._global_code = []
        self._init_calls = []
        self._final_init_calls = []
        self._global_resources = []
        self._modules_exprs = []
    
    def add_subst (self, key, val, indent=-1):
        self._subst[key] = {'val':val, 'indent':indent}
    
    def add_config (self, name, dtype, value, is_constant=False):
        properties = []
        if is_constant:
            properties.append('ConfigPropertyConstant')
        self._config_options.append({'name':name, 'dtype':dtype, 'value':value, 'properties':properties})
        return name
    
    def add_float_config (self, name, value, **kwargs):
        return self.add_config(name, 'DOUBLE', format_cpp_float(value), **kwargs)
    
    def add_bool_config (self, name, value, **kwargs):
        return self.add_config(name, 'BOOL', 'true' if value else 'false', **kwargs)
    
    def add_float_constant (self, name, value):
        self._constants.append({'type':'using', 'name':name, 'value':'AMBRO_WRAP_DOUBLE({})'.format(format_cpp_float(value))})
        return name
    
    def add_typedef (self, name, value):
        self._constants.append({'type':'using', 'name':name, 'value':value})
        return name
    
    def add_int_constant (self, dtype, name, value):
        m = re.match('\\A(u?)int(8|16|32|64)\\Z', dtype)
        assert m
        u = m.group(1)
        b = m.group(2)
        self._constants.append({'type':'static {}_t const'.format(dtype), 'name':name, 'value':'{}INT{}_C({})'.format(u.upper(), b, value)})
        return name
    
    def add_platform_include (self, inc_file):
        self._platform_includes.append(inc_file)
    
    def add_aprinter_include (self, inc_file):
        self._aprinter_includes.add(inc_file)
    
    def register_objects (self, kind, config, key):
        if kind not in self._objects:
            self._objects[kind] = {}
        for obj_config in config.iter_list_config(key, max_count=20):
            name = obj_config.get_string('Name')
            if name in self._objects[kind]:
                obj_config.path().error('Duplicate {} name'.format(kind))
            self._objects[kind][name] = obj_config
    
    def get_object (self, kind, config, key):
        name = config.get_string(key)
        if kind not in self._objects or name not in self._objects[kind]:
            config.key_path(key).error('Nonexistent {} specified'.format(kind))
        return self._objects[kind][name]
    
    def register_singleton_object (self, kind, value):
        assert kind not in self._singleton_objects
        self._singleton_objects[kind] = value
        return value
    
    def get_singleton_object (self, kind):
        assert kind in self._singleton_objects
        return self._singleton_objects[kind]
    
    def add_global_code (self, priority, code):
        self._global_code.append({'priority':priority, 'code':code})
    
    def add_isr (self, isr):
        self.add_global_code(-1, isr)
    
    def add_init_call (self, priority, init_call):
        self._init_calls.append({'priority':priority, 'init_call':init_call})
    
    def add_final_init_call (self, priority, init_call):
        self._final_init_calls.append({'priority':priority, 'init_call':init_call})
    
    def add_finalize_action (self, action):
        self._finalize_actions.append(action)
    
    def add_global_resource (self, priority, name, expr, context_name=None, code_before=None, code_before_program=None, extra_program_child=None):
        code_before = '' if code_before is None else '{}\n'.format(code_before)
        self._global_resources.append({
            'priority':priority, 'name':name, 'expr':expr, 'context_name':context_name,
            'code_before':code_before, 'code_before_program':code_before_program,
            'extra_program_child':extra_program_child,
        })
    
    def add_module (self):
        index = len(self._modules_exprs)
        self._modules_exprs.append(None)
        return GenPrinterModule(self, index)
    
    def get_modules_exprs (self):
        return self._modules_exprs
    
    def finalize (self):
        for action in reversed(self._finalize_actions):
            action()
        
        for so in self._singleton_objects.itervalues():
            if hasattr(so, 'finalize'):
                so.finalize()
        
        global_resources = sorted(self._global_resources, key=lambda x: x['priority'])
        
        program_children = []
        program_children.extend(gr['name'] for gr in global_resources)
        program_children.extend(gr['extra_program_child'] for gr in global_resources if gr['extra_program_child'] is not None)
        
        self.add_subst('GENERATED_WARNING', 'WARNING: This file was automatically generated!')
        self.add_subst('EXTRA_CONSTANTS', ''.join('{} {} = {};\n'.format(c['type'], c['name'], c['value']) for c in self._constants))
        self.add_subst('ConfigOptions', ''.join('APRINTER_CONFIG_OPTION_{}({}, {}, ConfigProperties<{}>)\n'.format(c['dtype'], c['name'], c['value'], ', '.join(c['properties'])) for c in self._config_options))
        self.add_subst('PLATFORM_INCLUDES', ''.join('#include <{}>\n'.format(inc) for inc in self._platform_includes))
        self.add_subst('AprinterIncludes', ''.join('#include <aprinter/{}>\n'.format(inc) for inc in sorted(self._aprinter_includes)))
        self.add_subst('GlobalCode', ''.join('{}\n'.format(gc['code']) for gc in sorted(self._global_code, key=lambda x: x['priority'])))
        self.add_subst('InitCalls', ''.join('    {}\n'.format(ic['init_call']) for ic in sorted(self._init_calls, key=lambda x: x['priority'])))
        self.add_subst('GlobalResourceExprs', ''.join('{}using {} = {};\n'.format(gr['code_before'], gr['name'], gr['expr'].build(indent=0)) for gr in global_resources))
        self.add_subst('GlobalResourceContextAliases', ''.join('    using {} = {};\n'.format(gr['context_name'], gr['name']) for gr in global_resources if gr['context_name'] is not None))
        self.add_subst('GlobalResourceProgramChildren', ',\n'.join('    {}'.format(pc_name) for pc_name in program_children))
        self.add_subst('GlobalResourceInit', ''.join('    {}::init(c);\n'.format(gr['name']) for gr in global_resources))
        self.add_subst('FinalInitCalls', ''.join('    {}\n'.format(ic['init_call']) for ic in sorted(self._final_init_calls, key=lambda x: x['priority'])))
        self.add_subst('CodeBeforeProgram', ''.join('{}\n'.format(gr['code_before_program']) for gr in global_resources if gr['code_before_program'] is not None))
    
    def get_subst (self):
        res = {}
        for (key, subst) in self._subst.iteritems():
            val = subst['val']
            indent = subst['indent']
            res[key] = val if type(val) is str else val.build(indent)
        return res

class GenPrinterModule(object):
    def __init__ (self, gen, index):
        self._gen = gen
        self._index = index
    
    @property
    def index (self):
        return self._index
    
    def set_expr (self, expr):
        self._gen._modules_exprs[self._index] = expr

class GenConfigReader(config_reader.ConfigReader):
    def get_int_constant (self, key):
        return str(self.get_int(key))
    
    def get_bool_constant (self, key):
        return 'true' if self.get_bool(key) else 'false'

    def get_float_constant (self, key):
        return format_cpp_float(self.get_float(key))

    def get_identifier (self, key, validate=None):
        val = self.get_string(key)
        if not re.match(IDENTIFIER_REGEX, val):
            self.key_path(key).error('Incorrect format.')
        if validate is not None and not validate(val):
            self.key_path(key).error('Custom validation failed.')
        return val
    
    def get_id_char (self, key):
        val = self.get_string(key)
        if val not in string.ascii_uppercase:
            self.key_path(key).error('Incorrect format.')
        return val
    
    def do_selection (self, key, sel_def):
        for config in self.enter_config(key):
            try:
                result = sel_def.run(config.get_string('_compoundName'), config)
            except selection.SelectionError:
                config.path().error('Unknown choice.')
            return result
    
    def do_list (self, key, elem_cb, min_count=-1, max_count=-1):
        elems = []
        for (i, config) in enumerate(self.iter_list_config(key, min_count=min_count, max_count=max_count)):
            elems.append(elem_cb(config, i))
        return TemplateList(elems)
    
    def do_keyed_list (self, count, elems_key, elem_key_prefix, elem_cb):
        elems = []
        elems_config = self.get_config(elems_key)
        for i in range(count):
            elem_config = elems_config.get_config('{}{}'.format(elem_key_prefix, i))
            elems.append(elem_cb(elem_config, i))
        return TemplateList(elems)
    
    def do_enum (self, key, mapping):
        val = self.get_string(key)
        if val not in mapping:
            self.key_path(key).error('Incorrect choice.')
        return mapping[val]

class TemplateExpr(object):
    def __init__ (self, name, args):
        self._name = name
        self._args = args
    
    def append_arg(self, arg):
        self._args.append(arg)
    
    def build (self, indent):
        if indent == -1 or len(self._args) == 0:
            initiator = ''
            separator = ', '
            terminator = ''
            child_indent = -1
        else:
            initiator = '\n' + '    ' * (indent + 1)
            separator = ',' + initiator
            terminator = '\n' + '    ' * indent
            child_indent = indent + 1
        return '{}<{}{}{}>'.format(self._name, initiator, separator.join(_build_template_arg(arg, child_indent) for arg in self._args), terminator)

def _build_template_arg (arg, indent):
    if type(arg) is str or type(arg) is int or type(arg) is long:
        return str(arg)
    if type(arg) is bool:
        return 'true' if arg else 'false'
    return arg.build(indent)

class TemplateList(TemplateExpr):
    def __init__ (self, args):
        TemplateExpr.__init__(self, 'MakeTypeList', args)

class TemplateChar(object):
    def __init__ (self, ch):
        self._ch = ch
    
    def build (self, indent):
        return '\'{}\''.format(self._ch)

def format_cpp_float(value):
    return '{:.17E}'.format(value).replace('INF', 'INFINITY')

def setup_event_loop(gen):
    gen.add_aprinter_include('system/BusyEventLoop.h')
    
    code_before_expr = 'struct MyLoopExtraDelay;'
    expr = TemplateExpr('BusyEventLoop', ['MyContext', 'Program', 'MyLoopExtraDelay'])
    code_before_program  = 'using MyLoopExtra = BusyEventLoopExtra<Program, MyLoop, typename MyPrinter::EventLoopFastEvents>;\n'
    code_before_program += 'struct MyLoopExtraDelay : public WrapType<MyLoopExtra> {};'
    
    gen.add_global_resource(0, 'MyLoop', expr, context_name='EventLoop', code_before=code_before_expr, code_before_program=code_before_program, extra_program_child='MyLoopExtra')
    gen.add_final_init_call(100, 'MyLoop::run(c);')

def setup_platform(gen, config, key):
    platform_sel = selection.Selection()
    
    @platform_sel.option('At91Sam3x8e')
    def option(platform):
        gen.add_platform_include('aprinter/platform/at91sam3x/at91sam3x_support.h')
        gen.add_init_call(-1, 'platform_init();')
    
    @platform_sel.option('At91Sam3u4e')
    def option(platform):
        gen.add_platform_include('aprinter/platform/at91sam3u/at91sam3u_support.h')
        gen.add_init_call(-1, 'platform_init();')
    
    @platform_sel.option('Teensy3')
    def option(platform):
        gen.add_platform_include('aprinter/platform/teensy3/teensy3_support.h')
    
    @platform_sel.options(['AVR ATmega2560', 'AVR ATmega1284p'])
    def option(platform_name, platform):
        gen.add_platform_include('avr/io.h')
        gen.add_platform_include('aprinter/platform/avr/avr_support.h')
        gen.add_init_call(-3, 'sei();')
    
    @platform_sel.option('Stm32f4')
    def option(platform):
        gen.add_platform_include('aprinter/platform/stm32f4/stm32f4_support.h')
        gen.add_init_call(-1, 'platform_init();')
        gen.add_final_init_call(-1, 'platform_init_final();')
    
    config.do_selection(key, platform_sel)

def setup_debug_interface(gen, config, key):
    debug_sel = selection.Selection()
    
    @debug_sel.option('NoDebug')
    def option(debug):
        pass
    
    @debug_sel.option('ArmItmDebug')
    def option(debug):
        stimulus_port = debug.get_int('StimulusPort')
        if not 0 <= stimulus_port <= 31:
            debug.key_path('StimulusPort').error('Incorrect value.')
        gen.add_platform_include('aprinter/hal/generic/ArmItmDebug.h')
        gen.add_aprinter_include('hal/generic/NewlibDebugWrite.h')
        gen.add_global_code(0, 'using MyDebug = ArmItmDebug<MyContext, {}>;'.format(
            stimulus_port,
        ))
        gen.add_global_code(0, 'APRINTER_SETUP_NEWLIB_DEBUG_WRITE(MyDebug::write, MyContext())')
    
    config.do_selection(key, debug_sel)

class CommonClock(object):
    def __init__ (self, gen, config, clock_name, priority, clockdef_func):
        self._gen = gen
        self._config = config
        self._clock_name = clock_name
        self._my_clock = 'My{}'.format(clock_name)
        self._priority = priority
        self._clockdef = function_defined.FunctionDefinedClass(clockdef_func)
        gen.add_aprinter_include(self._clockdef.INCLUDE)
        self._timers = self._load_timers(config)
        self._interrupt_timers = []
        self._primary_timer = self.check_timer(config.get_string('primary_timer'), config.key_path('primary_timer'))
    
    def _load_timers (self, config):
        timers = {}
        if hasattr(self._clockdef, 'handle_timer'):
            for timer_config in config.iter_list_config('timers', max_count=20):
                timer_id = self.check_timer(timer_config.get_string('Timer'), timer_config.key_path('Timer'))
                if timer_id in timers:
                    timer_config.path().error('Duplicate timer specified.')
                timers[timer_id] = self._clockdef.handle_timer(self._gen, timer_id, timer_config)
        return timers
    
    def check_timer (self, timer_name, path):
        match = re.match(self._clockdef.TIMER_RE, timer_name) 
        if not match:
            path.error('Incorrect timer name.')
        return match.group(1)
    
    def check_oc_unit (self, name, path):
        m = re.match(self._clockdef.CHANNEL_RE, name)
        if m is None:
            path.error('Incorrect OC unit format.')
        return {'tc':m.group(1), 'channel':m.group(2)}
    
    def add_interrupt_timer (self, name, user, clearance, path):
        it = self.check_oc_unit(name, path)
        self._interrupt_timers.append(it)
        clearance_extra = ', {}'.format(clearance) if clearance is not None else ''
        self._gen.add_isr(self._clockdef.INTERRUPT_TIMER_ISR(it, user))
        return self._clockdef.INTERRUPT_TIMER_EXPR(it, clearance_extra)
    
    def finalize (self):
        auto_timers = (set(it['tc'] for it in self._interrupt_timers) | set([self._primary_timer])) - set(self._timers)
        for timer_id in auto_timers:
            self._timers[timer_id] = self._clockdef.TIMER_EXPR(timer_id)
            if hasattr(self._clockdef, 'TIMER_ISR'):
                self._gen.add_isr(self._clockdef.TIMER_ISR(self._my_clock, timer_id))
        
        if hasattr(self._clockdef, 'CLOCK_ISR'):
            clock = {'primary_timer': self._primary_timer}
            self._gen.add_isr(self._clockdef.CLOCK_ISR(self._my_clock, clock))
        
        temp_timers = set(self._timers)
        temp_timers.remove(self._primary_timer)
        ordered_timers = [self._primary_timer] + sorted(temp_timers)
        timers_expr = TemplateList([self._timers[timer_id] for timer_id in ordered_timers])
        
        clock_expr = self._clockdef.CLOCK_EXPR(self._config, timers_expr)
        
        self._gen.add_global_resource(self._priority, self._my_clock, clock_expr, context_name=self._clock_name)

def At91Sam3xClockDef(x):
    x.INCLUDE = 'hal/at91/At91Sam3xClock.h'
    x.CLOCK_EXPR = lambda config, timers: TemplateExpr('At91Sam3xClock', ['MyContext', 'Program', config.get_int_constant('prescaler'), timers])
    x.TIMER_RE = '\\ATC([0-9])\\Z'
    x.CHANNEL_RE = '\\ATC([0-9])([A-C])\\Z'
    x.INTERRUPT_TIMER_EXPR = lambda it, clearance_extra: 'At91Sam3xClockInterruptTimerService<At91Sam3xClockTC{}, At91Sam3xClockComp{}{}>'.format(it['tc'], it['channel'], clearance_extra)
    x.INTERRUPT_TIMER_ISR = lambda it, user: 'AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClockTC{}, At91Sam3xClockComp{}, {}, MyContext())'.format(it['tc'], it['channel'], user)
    x.TIMER_EXPR = lambda tc: 'At91Sam3xClockTC{}'.format(tc)
    x.TIMER_ISR = lambda my_clock, tc: 'AMBRO_AT91SAM3X_CLOCK_TC{}_GLOBAL({}, MyContext())'.format(tc, my_clock)

def At91Sam3uClockDef(x):
    x.INCLUDE = 'hal/at91/At91Sam3uClock.h'
    x.CLOCK_EXPR = lambda config, timers: TemplateExpr('At91Sam3uClock', ['MyContext', 'Program', config.get_int_constant('prescaler'), timers])
    x.TIMER_RE = '\\ATC([0-9])\\Z'
    x.CHANNEL_RE = '\\ATC([0-9])([A-C])\\Z'
    x.INTERRUPT_TIMER_EXPR = lambda it, clearance_extra: 'At91Sam3uClockInterruptTimerService<At91Sam3uClockTC{}, At91Sam3uClockComp{}{}>'.format(it['tc'], it['channel'], clearance_extra)
    x.INTERRUPT_TIMER_ISR = lambda it, user: 'AMBRO_AT91SAM3U_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3uClockTC{}, At91Sam3uClockComp{}, {}, MyContext())'.format(it['tc'], it['channel'], user)
    x.TIMER_EXPR = lambda tc: 'At91Sam3uClockTC{}'.format(tc)
    x.TIMER_ISR = lambda my_clock, tc: 'AMBRO_AT91SAM3U_CLOCK_TC{}_GLOBAL({}, MyContext())'.format(tc, my_clock)

def Mk20ClockDef(x):
    x.INCLUDE = 'hal/teensy3/Mk20Clock.h'
    x.CLOCK_EXPR = lambda config, timers: TemplateExpr('Mk20Clock', ['MyContext', 'Program', config.get_int_constant('prescaler'), timers])
    x.TIMER_RE = '\\AFTM([0-9])\\Z'
    x.CHANNEL_RE = '\\AFTM([0-9])_([0-9])\\Z'
    x.INTERRUPT_TIMER_EXPR = lambda it, clearance_extra: 'Mk20ClockInterruptTimerService<Mk20ClockFTM{}, {}{}>'.format(it['tc'], it['channel'], clearance_extra)
    x.INTERRUPT_TIMER_ISR = lambda it, user: 'AMBRO_MK20_CLOCK_INTERRUPT_TIMER_GLOBAL(Mk20ClockFTM{}, {}, {}, MyContext())'.format(it['tc'], it['channel'], user)
    x.TIMER_EXPR = lambda tc: 'Mk20ClockFtmSpec<Mk20ClockFTM{}>'.format(tc)
    x.TIMER_ISR = lambda my_clock, tc: 'AMBRO_MK20_CLOCK_FTM_GLOBAL({}, {}, MyContext())'.format(tc, my_clock)

def AvrClockDef(x):
    x.INCLUDE = 'hal/avr/AvrClock.h'
    x.CLOCK_EXPR = lambda config, timers: TemplateExpr('AvrClock', ['MyContext', 'Program', config.get_int_constant('PrescaleDivide'), timers])
    x.TIMER_RE = '\\ATC([0-9])\\Z'
    x.CHANNEL_RE = '\\ATC([0-9])_([A-Z])\\Z'
    x.INTERRUPT_TIMER_EXPR = lambda it, clearance_extra: 'AvrClockInterruptTimerService<AvrClockTcChannel{}{}{}>'.format(it['tc'], it['channel'], clearance_extra)
    x.INTERRUPT_TIMER_ISR = lambda it, user: 'AMBRO_AVR_CLOCK_INTERRUPT_TIMER_ISRS({}, {}, {}, MyContext())'.format(it['tc'], it['channel'], user)
    x.TIMER_EXPR = lambda tc: 'AvrClockTcSpec<AvrClockTc{}>'.format(tc)
    x.CLOCK_ISR = lambda my_clock, clock: 'AMBRO_AVR_CLOCK_ISRS({}, {}, MyContext())'.format(clock['primary_timer'], my_clock)
    
    @assign_func.assign_func(x, 'handle_timer')
    def handle_timer(gen, timer_id, timer_config):
        mode_sel = selection.Selection()
        
        @mode_sel.option('AvrClockTcModeClock')
        def option(mode):
            return 'AvrClockTcModeClock'
        
        @mode_sel.option('AvrClockTcMode8BitPwm')
        def option(mode):
            return TemplateExpr('AvrClockTcMode8BitPwm', [
                mode.get_int('PrescaleDivide'),
            ])
        
        @mode_sel.option('AvrClockTcMode16BitPwm')
        def option(mode):
            return TemplateExpr('AvrClockTcMode16BitPwm', [
                mode.get_int('PrescaleDivide'),
                mode.get_int('TopVal'),
            ])
        
        return TemplateExpr('AvrClockTcSpec', [
            'AvrClockTc{}'.format(timer_id),
            timer_config.do_selection('Mode', mode_sel),
        ])

def Stm32f4ClockDef(x):
    x.INCLUDE = 'hal/stm32/Stm32f4Clock.h'
    x.CLOCK_EXPR = lambda config, timers: TemplateExpr('Stm32f4Clock', ['MyContext', 'Program', config.get_int_constant('prescaler'), timers])
    x.TIMER_RE = '\\ATIM([0-9]{1,2})\\Z'
    x.CHANNEL_RE = '\\ATIM([0-9]{1,2})_([1-4])\\Z'
    x.INTERRUPT_TIMER_EXPR = lambda it, clearance_extra: 'Stm32f4ClockInterruptTimerService<Stm32f4ClockTIM{}, Stm32f4ClockComp{}{}>'.format(it['tc'], it['channel'], clearance_extra)
    x.INTERRUPT_TIMER_ISR = lambda it, user: 'AMBRO_STM32F4_CLOCK_INTERRUPT_TIMER_GLOBAL(Stm32f4ClockTIM{}, Stm32f4ClockComp{}, {}, MyContext())'.format(it['tc'], it['channel'], user)
    x.TIMER_EXPR = lambda tc: 'Stm32f4ClockTIM{}'.format(tc)
    x.TIMER_ISR = lambda my_clock, tc: 'AMBRO_STM32F4_CLOCK_TC_GLOBAL({}, {}, MyContext())'.format(tc, my_clock)

def setup_clock(gen, config, key, clock_name, priority, allow_disabled):
    clock_sel = selection.Selection()
    
    @clock_sel.option('NoClock')
    def option(clock):
        if not allow_disabled:
            clock.path().error('A clock is required.')
        return None
    
    @clock_sel.option('At91Sam3xClock')
    def option(clock):
        return CommonClock(gen, clock, clock_name, priority, At91Sam3xClockDef)
    
    @clock_sel.option('At91Sam3uClock')
    def option(clock):
        return CommonClock(gen, clock, clock_name, priority, At91Sam3uClockDef)
    
    @clock_sel.option('Mk20Clock')
    def option(clock):
        return CommonClock(gen, clock, clock_name, priority, Mk20ClockDef)
    
    @clock_sel.option('AvrClock')
    def option(clock):
        return CommonClock(gen, clock, clock_name, priority, AvrClockDef)
    
    @clock_sel.option('Stm32f4Clock')
    def option(clock):
        return CommonClock(gen, clock, clock_name, priority, Stm32f4ClockDef)
    
    clock_object = config.do_selection(key, clock_sel)
    if clock_object is not None:
        gen.register_singleton_object(clock_name, clock_object)

def setup_pins (gen, config, key):
    pin_regexes = [IDENTIFIER_REGEX]
    
    pins_sel = selection.Selection()
    
    @pins_sel.option('At91SamPins')
    def options(pin_config):
        gen.add_aprinter_include('hal/at91/At91SamPins.h')
        pin_regexes.append('\\AAt91SamPin<At91SamPio[A-Z],[0-9]{1,3}>\\Z')
        return TemplateExpr('At91SamPins', ['MyContext', 'Program'])
    
    @pins_sel.option('Mk20Pins')
    def options(pin_config):
        gen.add_aprinter_include('hal/teensy3/Mk20Pins.h')
        pin_regexes.append('\\AMk20Pin<Mk20Port[A-Z],[0-9]{1,3}>\\Z')
        return TemplateExpr('Mk20Pins', ['MyContext', 'Program'])
    
    @pins_sel.option('AvrPins')
    def options(pin_config):
        gen.add_aprinter_include('hal/avr/AvrPins.h')
        pin_regexes.append('\\AAvrPin<AvrPort[A-Z],[0-9]{1,3}>\\Z')
        return TemplateExpr('AvrPins', ['MyContext', 'Program'])
    
    @pins_sel.option('Stm32f4Pins')
    def options(pin_config):
        gen.add_aprinter_include('hal/stm32/Stm32f4Pins.h')
        pin_regexes.append('\\AStm32f4Pin<Stm32f4Port[A-Z],[0-9]{1,3}>\\Z')
        return TemplateExpr('Stm32f4Pins', ['MyContext', 'Program'])
    
    gen.add_global_resource(10, 'MyPins', config.do_selection(key, pins_sel), context_name='Pins')
    gen.register_singleton_object('pin_regexes', pin_regexes)

def get_pin (gen, config, key):
    pin = config.get_string(key)
    pin_regexes = gen.get_singleton_object('pin_regexes')
    if not any(re.match(pin_regex, pin) for pin_regex in pin_regexes):
        config.key_path(key).error('Invalid pin value.')
    return pin

def setup_watchdog (gen, config, key, disable_watchdog, user):
    if disable_watchdog:
        gen.add_aprinter_include('hal/generic/NullWatchdog.h')
        return 'NullWatchdogService'
    
    watchdog_sel = selection.Selection()
    
    @watchdog_sel.option('At91SamWatchdog')
    def option(watchdog):
        gen.add_aprinter_include('hal/at91/At91SamWatchdog.h')
        return TemplateExpr('At91SamWatchdogService', [
            watchdog.get_int('Wdv')
        ])
    
    @watchdog_sel.option('Mk20Watchdog')
    def option(watchdog):
        gen.add_aprinter_include('hal/teensy3/Mk20Watchdog.h')
        gen.add_isr('AMBRO_MK20_WATCHDOG_GLOBAL({})'.format(user))
        return TemplateExpr('Mk20WatchdogService', [
            watchdog.get_int('Toval'),
            watchdog.get_int('Prescval'),
        ])
    
    @watchdog_sel.option('AvrWatchdog')
    def option(watchdog):
        wdto = watchdog.get_string('Timeout')
        if not re.match('\\AWDTO_[0-9A-Z]{1,10}\\Z', wdto):
            watchdog.key_path('Timeout').error('Incorrect value.')
        
        gen.add_aprinter_include('hal/avr/AvrWatchdog.h')
        gen.add_isr('AMBRO_AVR_WATCHDOG_GLOBAL')
        return TemplateExpr('AvrWatchdogService', [wdto])
    
    @watchdog_sel.option('Stm32f4Watchdog')
    def option(watchdog):
        gen.add_aprinter_include('hal/stm32/Stm32f4Watchdog.h')
        return TemplateExpr('Stm32f4WatchdogService', [
            watchdog.get_int('Divider'),
            watchdog.get_int('Reload'),
        ])
    
    return config.do_selection(key, watchdog_sel)

def setup_adc (gen, config, key):
    adc_sel = selection.Selection()
    
    @adc_sel.option('NoAdc')
    def option(adc_config):
        return None
    
    @adc_sel.option('At91SamAdc')
    def option(adc_config):
        gen.add_aprinter_include('hal/at91/At91SamAdc.h')
        gen.add_float_constant('AdcFreq', adc_config.get_float('freq'))
        gen.add_float_constant('AdcAvgInterval', adc_config.get_float('avg_interval'))
        gen.add_int_constant('uint16', 'AdcSmoothing', max(0, min(65535, int(adc_config.get_float('smoothing') * 65536.0))))
        gen.add_isr('AMBRO_AT91SAM_ADC_GLOBAL(MyAdc, MyContext())')
        
        return {
            'value_func': lambda pins: TemplateExpr('At91SamAdc', [
                'MyContext', 'Program', pins,
                TemplateExpr('At91SamAdcParams', [
                    'AdcFreq',
                    adc_config.get_int('startup'),
                    adc_config.get_int('settling'),
                    adc_config.get_int('tracking'),
                    adc_config.get_int('transfer'),
                    'At91SamAdcAvgParams<AdcAvgInterval>'
                ])
            ]),
            'pin_func': lambda pin: 'At91SamAdcSmoothPin<{}, AdcSmoothing>'.format(pin)
        }
    
    @adc_sel.option('At91Sam3uAdc')
    def option(adc_config):
        gen.add_aprinter_include('hal/at91/At91SamAdc.h')
        gen.add_float_constant('AdcFreq', adc_config.get_float('freq'))
        gen.add_float_constant('AdcAvgInterval', adc_config.get_float('avg_interval'))
        gen.add_int_constant('uint16', 'AdcSmoothing', max(0, min(65535, int(adc_config.get_float('smoothing') * 65536.0))))
        gen.add_isr('AMBRO_AT91SAM3U_ADC_GLOBAL(MyAdc, MyContext())')
        
        return {
            'value_func': lambda pins: TemplateExpr('At91SamAdc', [
                'MyContext', 'Program', pins,
                TemplateExpr('At91Sam3uAdcParams', [
                    'AdcFreq',
                    adc_config.get_int('startup'),
                    adc_config.get_int('shtim'),
                    'At91SamAdcAvgParams<AdcAvgInterval>'
                ])
            ]),
            'pin_func': lambda pin: 'At91SamAdcSmoothPin<{}, AdcSmoothing>'.format(pin)
        }
    
    @adc_sel.option('Mk20Adc')
    def option(adc_config):
        gen.add_aprinter_include('hal/teensy3/Mk20Adc.h')
        gen.add_int_constant('int32', 'AdcADiv', adc_config.get_int('AdcADiv'))
        gen.add_isr('AMBRO_MK20_ADC_ISRS(MyAdc, MyContext())')
        
        return {
            'value_func': lambda pins: TemplateExpr('Mk20Adc', ['MyContext', 'Program', pins, 'AdcADiv']),
            'pin_func': lambda pin: pin
        }
    
    @adc_sel.option('AvrAdc')
    def option(adc_config):
        gen.add_aprinter_include('hal/avr/AvrAdc.h')
        gen.add_int_constant('int32', 'AdcRefSel', adc_config.get_int('RefSel'))
        gen.add_int_constant('int32', 'AdcPrescaler', adc_config.get_int('Prescaler'))
        gen.add_isr('AMBRO_AVR_ADC_ISRS(MyAdc, MyContext())')
        
        return {
            'value_func': lambda pins: TemplateExpr('AvrAdc', ['MyContext', 'Program', pins, 'AdcRefSel', 'AdcPrescaler']),
            'pin_func': lambda pin: pin
        }
    
    @adc_sel.option('Stm32f4Adc')
    def option(adc_config):
        gen.add_aprinter_include('hal/stm32/Stm32f4Adc.h')
        gen.add_isr('APRINTER_STM32F4_ADC_GLOBAL(MyAdc, MyContext())')
        
        return {
            'value_func': lambda pins: TemplateExpr('Stm32f4Adc', [
                'MyContext', 'Program', pins,
                adc_config.get_int('ClockDivider'),
                adc_config.get_int('SampleTimeSelection'),
            ]),
            'pin_func': lambda pin: pin
        }
    
    result = config.do_selection(key, adc_sel)
    if result is None:
        return
    
    gen.register_singleton_object('adc_pins', [])
    
    def finalize():
        adc_pins = gen.get_singleton_object('adc_pins')
        pins_expr = TemplateList([result['pin_func'](pin) for pin in adc_pins])
        gen.add_global_resource(20, 'MyAdc', result['value_func'](pins_expr), context_name='Adc')
    
    gen.add_finalize_action(finalize)

def setup_pwm(gen, config, key):
    pwm_sel = selection.Selection()
    
    @pwm_sel.option('Disabled')
    def option(pwm_config):
        return None
    
    @pwm_sel.option('At91Sam3xPwm')
    def option(pwm_config):
        gen.add_aprinter_include('hal/at91/At91Sam3xPwm.h')
        
        return TemplateExpr('At91Sam3xPwm', ['MyContext', 'Program', TemplateExpr('At91Sam3xPwmParams', [
            pwm_config.get_int('PreA'),
            pwm_config.get_int('DivA'),
            pwm_config.get_int('PreB'),
            pwm_config.get_int('DivB'),
        ])])
    
    pwm_expr = config.do_selection(key, pwm_sel)
    if pwm_expr is not None:
        gen.add_global_resource(25, 'MyPwm', pwm_expr, context_name='Pwm')

def use_input_mode (config, key):
    im_sel = selection.Selection()
    
    @im_sel.option('At91SamPinInputMode')
    def option(im_config):
        return TemplateExpr('At91SamPinInputMode', [
            im_config.do_enum('PullMode', {'Normal': 'At91SamPinPullModeNormal', 'Pull-up': 'At91SamPinPullModePullUp'}),
        ])
    
    @im_sel.option('Stm32f4PinInputMode')
    def option(im_config):
        return TemplateExpr('Stm32f4PinInputMode', [
            im_config.do_enum('PullMode', {'Normal': 'Stm32f4PinPullModeNone', 'Pull-up': 'Stm32f4PinPullModePullUp', 'Pull-down': 'Stm32f4PinPullModePullDown'}),
        ])
    
    @im_sel.option('AvrPinInputMode')
    def option(im_config):
        return im_config.do_enum('PullMode', {'Normal': 'AvrPinInputModeNormal', 'Pull-up': 'AvrPinInputModePullUp'})
    
    @im_sel.option('Mk20PinInputMode')
    def option(im_config):
        return im_config.do_enum('PullMode', {'Normal': 'Mk20PinInputModeNormal', 'Pull-up': 'Mk20PinInputModePullUp', 'Pull-down': 'Mk20PinInputModePullDown'})
    
    return config.do_selection(key, im_sel)

def use_digital_input (gen, config, key):
    di = gen.get_object('digital_input', config, key)
    input_mode = use_input_mode(di, 'InputMode')
    return '{}, {}'.format(get_pin(gen, di, 'Pin'), _build_template_arg(input_mode, -1))

def use_analog_input (gen, config, key, user):
    ai = gen.get_object('analog_input', config, key)
    
    analog_input_sel = selection.Selection()
    
    @analog_input_sel.option('AdcAnalogInput')
    def option(analog_input):
        gen.add_aprinter_include('printer/analog_input/AdcAnalogInput.h')
        pin = get_pin(gen, analog_input, 'Pin')
        gen.get_singleton_object('adc_pins').append(pin)
        return TemplateExpr('AdcAnalogInputService', [pin])
    
    @analog_input_sel.option('Max31855AnalogInput')
    def option(analog_input):
        gen.add_aprinter_include('printer/analog_input/Max31855AnalogInput.h')
        return TemplateExpr('Max31855AnalogInputService', [
            get_pin(gen, analog_input, 'SsPin'),
            use_spi(gen, analog_input, 'SpiService', '{}::GetSpi'.format(user)),
        ])
    
    return ai.do_selection('Driver', analog_input_sel)

def use_interrupt_timer (gen, config, key, user, clearance=None):
    clock = gen.get_singleton_object('Clock')
    
    for it_config in config.enter_config(key):
        return clock.add_interrupt_timer(it_config.get_string('oc_unit'), user, clearance, it_config.path())

def use_pwm_output (gen, config, key, user, username, hard=False):
    pwm_output = gen.get_object('pwm_output', config, key)
    
    backend_sel = selection.Selection()
    
    @backend_sel.option('SoftPwm')
    def option(backend):
        if hard:
            config.path().error('Only Hard PWM is allowed here.')
        
        gen.add_aprinter_include('printer/pwm/SoftPwm.h')
        return TemplateExpr('SoftPwmService', [
            get_pin(gen, backend, 'OutputPin'),
            backend.get_bool('OutputInvert'),
            gen.add_float_constant('{}PulseInterval'.format(username), backend.get_float('PulseInterval')),
            use_interrupt_timer(gen, backend, 'Timer', '{}::TheTimer'.format(user))
        ])
    
    @backend_sel.option('HardPwm')
    def option(backend):
        gen.add_aprinter_include('printer/pwm/HardPwm.h')
        
        hard_driver_sel = selection.Selection()
        
        @hard_driver_sel.option('AvrClockPwm')
        def option(hard_driver):
            clock = gen.get_singleton_object('Clock')
            oc_unit = clock.check_oc_unit(hard_driver.get_string('oc_unit'), hard_driver.path())
            
            return TemplateExpr('AvrClockPwmService', [
                'AvrClockTcChannel{}{}'.format(oc_unit['tc'], oc_unit['channel']),
                get_pin(gen, hard_driver, 'OutputPin'),
            ])
        
        @hard_driver_sel.option('At91Sam3xPwmChannel')
        def option(hard_driver):
            return TemplateExpr('At91Sam3xPwmChannelService', [
                hard_driver.get_int('ChannelPrescaler'),
                hard_driver.get_int('ChannelPeriod'),
                hard_driver.get_int('ChannelNumber'),
                get_pin(gen, hard_driver, 'OutputPin'),
                TemplateChar(hard_driver.get_identifier('Signal')),
                hard_driver.get_bool('Invert'),
            ])
        
        hard_pwm_expr = backend.do_selection('HardPwmDriver', hard_driver_sel)
        
        if hard:
            return hard_pwm_expr
        
        return TemplateExpr('HardPwmService', [
            hard_pwm_expr
        ])
    
    return pwm_output.do_selection('Backend', backend_sel)

def use_spi (gen, config, key, user):
    spi_sel = selection.Selection()
    
    @spi_sel.option('At91SamSpi')
    def option(spi_config):
        gen.add_aprinter_include('hal/at91/At91SamSpi.h')
        devices = {
            'At91Sam3xSpiDevice':'AMBRO_AT91SAM3X_SPI_GLOBAL',
            'At91Sam3uSpiDevice':'AMBRO_AT91SAM3U_SPI_GLOBAL'
        }
        dev = spi_config.get_identifier('Device')
        if dev not in devices:
            spi_config.path().error('Incorrect SPI device.')
        gen.add_isr('{}({}, MyContext())'.format(devices[dev], user))
        return TemplateExpr('At91SamSpiService', [dev])
    
    @spi_sel.option('At91SamUsartSpi')
    def option(spi_config):
        gen.add_aprinter_include('hal/at91/At91SamUsartSpi.h')
        dev_index = spi_config.get_int('DeviceIndex')
        gen.add_isr('APRINTER_AT91SAM_USART_SPI_GLOBAL({}, {}, MyContext())'.format(dev_index, user))
        return TemplateExpr('At91SamUsartSpiService', [
            'At91SamUsartSpiDevice{}'.format(dev_index),
            spi_config.get_int('ClockDivider'),
        ])
    
    @spi_sel.option('AvrSpi')
    def option(spi_config):
        gen.add_aprinter_include('hal/avr/AvrSpi.h')
        gen.add_isr('AMBRO_AVR_SPI_ISRS({}, MyContext())'.format(user))
        return TemplateExpr('AvrSpiService', [spi_config.get_int('SpeedDiv')])
    
    return config.do_selection(key, spi_sel)

def use_sdio (gen, config, key, user):
    sdio_sel = selection.Selection()
    
    @sdio_sel.option('Stm32f4Sdio')
    def option(sdio_config):
        gen.add_aprinter_include('hal/stm32/Stm32f4Sdio.h')
        return TemplateExpr('Stm32f4SdioService', [
            sdio_config.get_bool('IsWideMode'),
            sdio_config.get_int('DataTimeoutBusClocks'),
        ])
    
    @sdio_sel.option('At91SamSdio')
    def option(sdio_config):
        gen.add_aprinter_include('hal/at91/At91SamSdio.h')
        return TemplateExpr('At91SamSdioService', [
            sdio_config.get_int('Slot'),
            sdio_config.get_bool('IsWideMode'),
        ])
    
    return config.do_selection(key, sdio_sel)

def use_i2c (gen, config, key, user, username):
    i2c_sel = selection.Selection()
    
    @i2c_sel.option('At91SamI2c')
    def option(i2c_config):
        gen.add_aprinter_include('hal/at91/At91SamI2c.h')
        devices = {
            'At91SamI2cDevice1':1,
        }
        dev = i2c_config.get_identifier('Device')
        if dev not in devices:
            i2c_config.path().error('Incorrect I2C device.')
        gen.add_isr('AMBRO_AT91SAM_I2C_GLOBAL({}, {}, MyContext())'.format(devices[dev], user))
        return 'At91SamI2cService<{}, {}, {}>'.format(
            dev,
            i2c_config.get_int('Ckdiv'),
            gen.add_float_constant('{}I2cFreq'.format(username), i2c_config.get_float('I2cFreq'))
        )
    
    return config.do_selection(key, i2c_sel)

def use_eeprom(gen, config, key, user):
    eeprom_sel = selection.Selection()
    
    @eeprom_sel.option('I2cEeprom')
    def option(eeprom):
        gen.add_aprinter_include('devices/I2cEeprom.h')
        return TemplateExpr('I2cEepromService', [use_i2c(gen, eeprom, 'I2c', '{}::GetI2c'.format(user), 'ConfigEeprom'), eeprom.get_int('I2cAddr'), eeprom.get_int('Size'), eeprom.get_int('BlockSize'), gen.add_float_constant('ConfigEepromWriteTimeout', eeprom.get_float('WriteTimeout'))])
    
    @eeprom_sel.option('TeensyEeprom')
    def option(eeprom):
        gen.add_aprinter_include('hal/teensy3/TeensyEeprom.h')
        return TemplateExpr('TeensyEepromService', [eeprom.get_int('Size'), eeprom.get_int('FakeBlockSize')])
    
    @eeprom_sel.option('AvrEeprom')
    def option(eeprom):
        gen.add_aprinter_include('hal/avr/AvrEeprom.h')
        gen.add_isr('AMBRO_AVR_EEPROM_ISRS({}, MyContext())'.format(user))
        return TemplateExpr('AvrEepromService', [eeprom.get_int('FakeBlockSize')])
    
    @eeprom_sel.option('FlashWrapper')
    def option(eeprom):
        gen.add_aprinter_include('devices/FlashWrapper.h')
        return TemplateExpr('FlashWrapperService', [
            use_flash(gen, eeprom, 'FlashDriver', '{}::GetFlash'.format(user)),
        ])
    
    return config.do_selection(key, eeprom_sel)

def use_flash(gen, config, key, user):
    flash_sel = selection.Selection()
    
    @flash_sel.option('At91SamFlash')
    def option(flash):
        device_index = flash.get_int('DeviceIndex')
        if not (0 <= device_index < 10):
            flash.key_path('DeviceIndex').error('Invalid device index.')
        gen.add_aprinter_include('hal/at91/At91Sam3xFlash.h')
        gen.add_isr('AMBRO_AT91SAM3X_FLASH_GLOBAL({}, {}, MyContext())'.format(device_index, user))
        return TemplateExpr('At91Sam3xFlashService', [
            'At91Sam3xFlashDevice{}'.format(device_index)
        ])
    
    return config.do_selection(key, flash_sel)

def use_serial(gen, config, key, user):
    serial_sel = selection.Selection()
    
    @serial_sel.option('AsfUsbSerial')
    def option(serial_service):
        gen.add_aprinter_include('hal/at91/AsfUsbSerial.h')
        gen.add_init_call(0, 'udc_start();')
        return 'AsfUsbSerialService'
    
    @serial_sel.option('At91Sam3xSerial')
    def option(serial_service):
        gen.add_aprinter_include('hal/at91/At91Sam3xSerial.h')
        gen.add_isr('AMBRO_AT91SAM3X_SERIAL_GLOBAL({}, MyContext())'.format(user))
        if serial_service.get_bool('UseForDebug'):
            gen.add_aprinter_include('hal/generic/NewlibDebugWrite.h')
            gen.add_global_code(0, 'APRINTER_SETUP_NEWLIB_DEBUG_WRITE(At91Sam3xSerial_DebugWrite<{}>, MyContext())'.format(user))
        return 'At91Sam3xSerialService'
    
    @serial_sel.option('TeensyUsbSerial')
    def option(serial_service):
        gen.add_aprinter_include('hal/teensy3/TeensyUsbSerial.h')
        gen.add_global_code(0, 'extern "C" { void usb_init (void); }')
        gen.add_init_call(0, 'usb_init();')
        return 'TeensyUsbSerialService'
    
    @serial_sel.option('AvrSerial')
    def option(serial_service):
        gen.add_aprinter_include('hal/avr/AvrSerial.h')
        gen.add_aprinter_include('hal/avr/AvrDebugWrite.h')
        gen.add_isr('AMBRO_AVR_SERIAL_ISRS({}, MyContext())'.format(user))
        gen.add_global_code(0, 'APRINTER_SETUP_AVR_DEBUG_WRITE(AvrSerial_DebugPutChar<{}>, MyContext())'.format(user))
        gen.add_init_call(-2, 'aprinter_init_avr_debug_write();')
        return TemplateExpr('AvrSerialService', [serial_service.get_bool('DoubleSpeed')])
    
    @serial_sel.option('Stm32f4UsbSerial')
    def option(serial_service):
        gen.add_aprinter_include('hal/stm32/Stm32f4UsbSerial.h')
        return 'Stm32f4UsbSerialService'
    
    @serial_sel.option('NullSerial')
    def option(serial_service):
        gen.add_aprinter_include('hal/generic/NullSerial.h')
        return 'NullSerialService'
    
    return config.do_selection(key, serial_sel)

def use_sdcard(gen, config, key, user):
    sd_service_sel = selection.Selection()

    @sd_service_sel.option('SpiSdCard')
    def option(spi_sd):
        gen.add_aprinter_include('devices/SpiSdCard.h')
        return TemplateExpr('SpiSdCardService', [
            get_pin(gen, spi_sd, 'SsPin'),
            use_spi(gen, spi_sd, 'SpiService', '{}::GetSpi'.format(user)),
        ])
    
    @sd_service_sel.option('SdioSdCard')
    def option(sdio_sd):
        gen.add_aprinter_include('devices/SdioSdCard.h')
        return TemplateExpr('SdioSdCardService', [
            use_sdio(gen, sdio_sd, 'SdioService', '{}::GetSdio'.format(user)),
        ])
    
    return config.do_selection(key, sd_service_sel)

def use_config_manager(gen, config, key, user):
    config_manager_sel = selection.Selection()
    
    @config_manager_sel.option('ConstantConfigManager')
    def option(config_manager):
        gen.add_aprinter_include('printer/config_manager/ConstantConfigManager.h')
        return 'ConstantConfigManagerService'
    
    @config_manager_sel.option('RuntimeConfigManager')
    def option(config_manager):
        gen.add_aprinter_include('printer/config_manager/RuntimeConfigManager.h')
        
        config_store_sel = selection.Selection()
        
        @config_store_sel.option('NoStore')
        def option(config_store):
            return 'RuntimeConfigManagerNoStoreService'
        
        @config_store_sel.option('EepromConfigStore')
        def option(config_store):
            gen.add_aprinter_include('printer/config_store/EepromConfigStore.h')
            
            return TemplateExpr('EepromConfigStoreService', [
                use_eeprom(gen, config_store, 'Eeprom', '{}::GetStore<>::GetEeprom'.format(user)),
                config_store.get_int('StartBlock'),
                config_store.get_int('EndBlock'),
            ])
        
        @config_store_sel.option('FileConfigStore')
        def option(config_store):
            gen.add_aprinter_include('printer/config_store/FileConfigStore.h')
            return 'FileConfigStoreService'
        
        return TemplateExpr('RuntimeConfigManagerService', [config_manager.do_selection('ConfigStore', config_store_sel)])
    
    return config.do_selection(key, config_manager_sel)

def use_microstep(gen, config, key):
    ms_driver_sel = selection.Selection()
    
    @ms_driver_sel.option('A4982')
    def option(ms_driver_config):
        gen.add_aprinter_include('printer/microstep/A4982MicroStep.h')
        
        return 'A4982MicroStep, A4982MicroStepParams<{}, {}>'.format(
            get_pin(gen, ms_driver_config, 'Ms1Pin'),
            get_pin(gen, ms_driver_config, 'Ms2Pin'),
        )
    
    @ms_driver_sel.option('A4988')
    def option(ms_driver_config):
        gen.add_aprinter_include('printer/microstep/A4988MicroStep.h')
        
        return 'A4988MicroStep, A4988MicroStepParams<{}, {}>'.format(
            get_pin(gen, ms_driver_config, 'Ms1Pin'),
            get_pin(gen, ms_driver_config, 'Ms2Pin'),
            get_pin(gen, ms_driver_config, 'Ms3Pin'),
        )
    
    return config.do_selection(key, ms_driver_sel)

def use_current_driver(gen, config, key, user):
    current_driver_sel = selection.Selection()
    
    @current_driver_sel.option('Ad5206Current')
    def option(current_driver):
        gen.add_aprinter_include('printer/current/Ad5206Current.h')
        
        return TemplateExpr('Ad5206CurrentService', [
            get_pin(gen, current_driver, 'SsPin'),
            use_spi(gen, current_driver, 'SpiService', '{}::GetSpi'.format(user)),
        ])
    
    return config.do_selection(key, current_driver_sel)

def use_current_driver_channel(gen, config, key, name):
    current_driver_channel_sel = selection.Selection()
    
    @current_driver_channel_sel.option('Ad5206CurrentChannelParams')
    def option(current_driver_channel):
        return TemplateExpr('Ad5206CurrentChannelParams', [
            current_driver_channel.get_int('DevChannelIndex'),
            gen.add_float_config('{}CurrentConversionFactor'.format(name), current_driver_channel.get_float('ConversionFactor'))
        ])
    
    return config.do_selection(key, current_driver_channel_sel)

def get_letter_number_name(config, key):
    name = config.get_string(key)
    match = re.match('\\A([A-Z])([0-9]{0,2})\\Z', name)
    if not match:
        config.key_path(key).error('Incorrect name (expecting letter optionally followed by a number).')
    letter = match.group(1)
    number = int(match.group(2)) if match.group(2) != '' else 0
    normalized_name = '{}{}'.format(letter, number) if number != 0 else letter
    return normalized_name, TemplateExpr('AuxControlName', [TemplateChar(letter), number])

def generate(config_root_data, cfg_name, main_template):
    gen = GenState()
    
    for config_root in config_reader.start(config_root_data, config_reader_class=GenConfigReader):
        if cfg_name is None:
            cfg_name = config_root.get_string('selected_config')
        
        for config in config_root.enter_elem_by_id('configurations', 'name', cfg_name):
            board_name = config.get_string('board')
            
            setup_event_loop(gen)
            
            aux_control_module = gen.add_module()
            aux_control_module_user = 'MyPrinter::GetModule<{}>'.format(aux_control_module.index)
            
            for board_data in config_root.enter_elem_by_id('boards', 'name', board_name):
                for platform_config in board_data.enter_config('platform_config'):
                    board_for_build = platform_config.get_string('board_for_build')
                    if not re.match('\\A[a-zA-Z0-9_]{1,128}\\Z', board_for_build):
                        platform_config.key_path('board_for_build').error('Incorrect format.')
                    gen.add_subst('BoardForBuild', board_for_build)
                    
                    output_type = platform_config.get_string('output_type')
                    if output_type not in ('hex', 'bin', 'elf'):
                        platform_config.key_path('output_type').error('Incorrect value.')
                    
                    setup_platform(gen, platform_config, 'platform')
                    
                    for platform in platform_config.enter_config('platform'):
                        setup_clock(gen, platform, 'clock', clock_name='Clock', priority=-10, allow_disabled=False)
                        setup_pins(gen, platform, 'pins')
                        setup_adc(gen, platform, 'adc')
                        if platform.has('pwm'):
                            setup_pwm(gen, platform, 'pwm')
                        if platform.has('fast_clock'):
                            setup_clock(gen, platform, 'fast_clock', clock_name='FastClock', priority=-12, allow_disabled=True)
                    
                    setup_debug_interface(gen, platform_config, 'debug_interface')
                    
                    for helper_name in platform_config.get_list(config_reader.ConfigTypeString(), 'board_helper_includes', max_count=20):
                        if not re.match('\\A[a-zA-Z0-9_]{1,128}\\Z', helper_name):
                            platform_config.key_path('board_helper_includes').error('Invalid helper name.')
                        gen.add_aprinter_include('board/{}.h'.format(helper_name))
                
                gen.register_objects('digital_input', board_data, 'digital_inputs')
                gen.register_objects('stepper_port', board_data, 'stepper_ports')
                gen.register_objects('analog_input', board_data, 'analog_inputs')
                gen.register_objects('pwm_output', board_data, 'pwm_outputs')
                gen.register_objects('laser_port', board_data, 'laser_ports')
                
                led_pin_expr = get_pin(gen, board_data, 'LedPin')
                event_channel_timer_expr = use_interrupt_timer(gen, board_data, 'EventChannelTimer', user='{}::GetEventChannelTimer<>'.format(aux_control_module_user), clearance='EventChannelTimerClearance')
                
                for performance in board_data.enter_config('performance'):
                    gen.add_typedef('TheAxisDriverPrecisionParams', performance.get_identifier('AxisDriverPrecisionParams'))
                    gen.add_float_constant('EventChannelTimerClearance', performance.get_float('EventChannelTimerClearance'))
                    optimize_for_size = performance.get_bool('OptimizeForSize')
                
                for development in board_data.enter_config('development'):
                    assertions_enabled = development.get_bool('AssertionsEnabled')
                    event_loop_benchmark_enabled = development.get_bool('EventLoopBenchmarkEnabled')
                    detect_overload_enabled = development.get_bool('DetectOverloadEnabled')
                    disable_watchdog = development.get_bool('DisableWatchdog')
                    build_with_clang = development.get_bool('BuildWithClang')
                    verbose_build = development.get_bool('VerboseBuild')
                    debug_symbols = development.get_bool('DebugSymbols')
                
                for serial in board_data.enter_config('serial'):
                    gen.add_aprinter_include('printer/SerialModule.h')
                    
                    serial_module = gen.add_module()
                    serial_user = 'MyPrinter::GetModule<{}>::GetSerial'.format(serial_module.index)
                    
                    serial_module.set_expr(TemplateExpr('SerialModuleService', [
                        'UINT32_C({})'.format(serial.get_int_constant('BaudRate')),
                        serial.get_int_constant('RecvBufferSizeExp'),
                        serial.get_int_constant('SendBufferSizeExp'),
                        TemplateExpr('GcodeParserParams', [serial.get_int_constant('GcodeMaxParts')]),
                        use_serial(gen, serial, 'Service', serial_user),
                    ]))
                
                sdcard_sel = selection.Selection()
                
                @sdcard_sel.option('NoSdCard')
                def option(sdcard):
                    pass
                
                @sdcard_sel.option('SdCard')
                def option(sdcard):
                    gen.add_aprinter_include('printer/SdCardModule.h')
                    
                    sdcard_module = gen.add_module()
                    sdcard_user = 'MyPrinter::GetModule<{}>::GetInput::GetSdCard'.format(sdcard_module.index)
                    
                    gcode_parser_sel = selection.Selection()
                    
                    @gcode_parser_sel.option('TextGcodeParser')
                    def option(parser):
                        gen.add_aprinter_include('printer/GcodeParser.h')
                        return 'FileGcodeParser, GcodeParserParams<{}>'.format(parser.get_int('MaxParts'))
                    
                    @gcode_parser_sel.option('BinaryGcodeParser')
                    def option(parser):
                        gen.add_aprinter_include('printer/BinaryGcodeParser.h')
                        return 'BinaryGcodeParser, BinaryGcodeParserParams<{}>'.format(parser.get_int('MaxParts'))
                    
                    fs_sel = selection.Selection()
                    
                    @fs_sel.option('Raw')
                    def option(fs_config):
                        gen.add_aprinter_include('printer/input/SdRawInput.h')
                        return TemplateExpr('SdRawInputService', [
                            use_sdcard(gen, sdcard, 'SdCardService', sdcard_user),
                        ])
                    
                    @fs_sel.option('Fat32')
                    def option(fs_config):
                        max_filename_size = fs_config.get_int('MaxFileNameSize')
                        if not (12 <= max_filename_size <= 1024):
                            fs_config.key_path('MaxFileNameSize').error('Bad value.')
                        
                        num_cache_entries = fs_config.get_int('NumCacheEntries')
                        if not (1 <= num_cache_entries <= 64):
                            fs_config.key_path('NumCacheEntries').error('Bad value.')
                        
                        gen.add_aprinter_include('printer/input/SdFatInput.h')
                        gen.add_aprinter_include('fs/FatFs.h')
                        
                        if fs_config.get_bool('EnableFsTest'):
                            gen.add_aprinter_include('printer/FsTestModule.h')
                            fs_test_module = gen.add_module()
                            fs_test_module.set_expr('FsTestModuleService')
                        
                        return TemplateExpr('SdFatInputService', [
                            use_sdcard(gen, sdcard, 'SdCardService', sdcard_user),
                            TemplateExpr('FatFsService', [
                                max_filename_size,
                                num_cache_entries,
                                fs_config.get_bool_constant('CaseInsensFileName'),
                                fs_config.get_bool_constant('FsWritable'),
                            ]),
                            fs_config.get_bool_constant('HaveAccessInterface'),
                        ])
                    
                    sdcard_module.set_expr(TemplateExpr('SdCardModuleService', [
                        sdcard.do_selection('FsType', fs_sel),
                        sdcard.do_selection('GcodeParser', gcode_parser_sel),
                        sdcard.get_int('BufferBaseSize'),
                        sdcard.get_int('MaxCommandSize'),
                    ]))
                
                board_data.get_config('sdcard_config').do_selection('sdcard', sdcard_sel)
                
                config_manager_expr = use_config_manager(gen, board_data.get_config('runtime_config'), 'config_manager', 'MyPrinter::GetConfigManager')
                
                current_config = board_data.get_config('current_config')
            
            gen.add_aprinter_include('printer/PrinterMain.h')
            gen.add_float_constant('FanSpeedMultiply', 1.0 / 255.0)
            
            for advanced in config.enter_config('advanced'):
                gen.add_float_constant('LedBlinkInterval', advanced.get_float('LedBlinkInterval'))
                gen.add_float_config('ForceTimeout', advanced.get_float('ForceTimeout'))
            
            current_control_channel_list = []
            
            def stepper_cb(stepper, stepper_index):
                name = stepper.get_id_char('Name')
                
                homing_sel = selection.Selection()
                
                @homing_sel.option('no_homing')
                def option(homing):
                    return 'PrinterMainNoHomingParams'
                
                @homing_sel.option('homing')
                def option(homing):
                    gen.add_aprinter_include('printer/AxisHomer.h')
                    
                    return TemplateExpr('PrinterMainHomingParams', [
                        gen.add_bool_config('{}HomeDir'.format(name), homing.get_bool('HomeDir')),
                        gen.add_float_config('{}HomeOffset'.format(name), homing.get_float('HomeOffset')),
                        TemplateExpr('AxisHomerService', [
                            use_digital_input(gen, homing, 'HomeEndstopInput'),
                            gen.add_bool_config('{}HomeEndInvert'.format(name), homing.get_bool('HomeEndInvert')),
                            gen.add_float_config('{}HomeFastMaxDist'.format(name), homing.get_float('HomeFastMaxDist')),
                            gen.add_float_config('{}HomeRetractDist'.format(name), homing.get_float('HomeRetractDist')),
                            gen.add_float_config('{}HomeSlowMaxDist'.format(name), homing.get_float('HomeSlowMaxDist')),
                            gen.add_float_config('{}HomeFastSpeed'.format(name), homing.get_float('HomeFastSpeed')),
                            gen.add_float_config('{}HomeRetractSpeed'.format(name), homing.get_float('HomeRetractSpeed')),
                            gen.add_float_config('{}HomeSlowSpeed'.format(name), homing.get_float('HomeSlowSpeed')),
                        ])
                    ])
                
                stepper_port = gen.get_object('stepper_port', stepper, 'stepper_port')
                
                if stepper_port.get_config('StepperTimer').get_string('_compoundName') != 'interrupt_timer':
                    stepper.key_path('stepper_port').error('Selected stepper port must have a timer unit defined.')
                
                gen.add_aprinter_include('driver/AxisDriver.h')
                
                def slave_steppers_cb(slave_stepper, slave_stepper_index):
                    slave_stepper_port = gen.get_object('stepper_port', slave_stepper, 'stepper_port')
                    
                    return TemplateExpr('PrinterMainSlaveStepperParams', [
                        get_pin(gen, slave_stepper_port, 'DirPin'),
                        get_pin(gen, slave_stepper_port, 'StepPin'),
                        get_pin(gen, slave_stepper_port, 'EnablePin'),
                        gen.add_bool_config('{}S{}InvertDir'.format(name, 1 + slave_stepper_index), slave_stepper.get_bool('InvertDir')),
                    ])
                
                microstep_sel = selection.Selection()
                
                @microstep_sel.option('NoMicroStep')
                def option(microstep_config):
                    return 'PrinterMainNoMicroStepParams'
                
                @microstep_sel.option('MicroStep')
                def option(microstep_config):
                    return TemplateExpr('PrinterMainMicroStepParams', [
                        use_microstep(gen, microstep_config, 'MicroStepDriver'),
                        stepper.get_int('MicroSteps'),
                    ])
                
                stepper_current_sel = selection.Selection()
                
                @stepper_current_sel.option('NoCurrent')
                def option(stepper_current_config):
                    pass
                
                @stepper_current_sel.option('Current')
                def option(stepper_current_config):
                    stepper_current_expr = TemplateExpr('MotorCurrentAxisParams', [
                        TemplateChar(name),
                        gen.add_float_config('{}Current'.format(name), stepper.get_float('Current')),
                        use_current_driver_channel(gen, stepper_current_config, 'DriverChannelParams', name),
                    ])
                    current_control_channel_list.append(stepper_current_expr)
                
                stepper_port.do_selection('current', stepper_current_sel)
                
                delay_sel = selection.Selection()
                
                @delay_sel.option('NoDelay')
                def option(delay_config):
                    return 'AxisDriverNoDelayParams'
                
                @delay_sel.option('Delay')
                def option(delay_config):
                    return TemplateExpr('AxisDriverDelayParams', [
                        gen.add_float_constant('{}DirSetTime'.format(name), delay_config.get_float('DirSetTime')),
                        gen.add_float_constant('{}StepHighTime'.format(name), delay_config.get_float('StepHighTime')),
                        gen.add_float_constant('{}StepLowTime'.format(name), delay_config.get_float('StepLowTime')),
                    ])
                
                return TemplateExpr('PrinterMainAxisParams', [
                    TemplateChar(name),
                    get_pin(gen, stepper_port, 'DirPin'),
                    get_pin(gen, stepper_port, 'StepPin'),
                    get_pin(gen, stepper_port, 'EnablePin'),
                    gen.add_bool_config('{}InvertDir'.format(name), stepper.get_bool('InvertDir')),
                    gen.add_float_config('{}StepsPerUnit'.format(name), stepper.get_float('StepsPerUnit')),
                    gen.add_float_config('{}MinPos'.format(name), stepper.get_float('MinPos')),
                    gen.add_float_config('{}MaxPos'.format(name), stepper.get_float('MaxPos')),
                    gen.add_float_config('{}MaxSpeed'.format(name), stepper.get_float('MaxSpeed')),
                    gen.add_float_config('{}MaxAccel'.format(name), stepper.get_float('MaxAccel')),
                    gen.add_float_config('{}DistanceFactor'.format(name), stepper.get_float('DistanceFactor')),
                    gen.add_float_config('{}CorneringDistance'.format(name), stepper.get_float('CorneringDistance')),
                    stepper.do_selection('homing', homing_sel),
                    stepper.get_bool('EnableCartesianSpeedLimit'),
                    stepper.get_bool('IsExtruder'),
                    32,
                    TemplateExpr('AxisDriverService', [
                        use_interrupt_timer(gen, stepper_port, 'StepperTimer', user='MyPrinter::GetAxisTimer<{}>'.format(stepper_index)),
                        'TheAxisDriverPrecisionParams',
                        stepper_port.get_bool('PreloadCommands'),
                        stepper_port.do_selection('delay', delay_sel),
                    ]),
                    stepper_port.do_selection('microstep', microstep_sel),
                    stepper.do_list('slave_steppers', slave_steppers_cb, max_count=10),
                ])
            
            steppers_expr = config.do_list('steppers', stepper_cb, min_count=1, max_count=15)
            
            def heater_cb(heater, heater_index):
                name, name_expr = get_letter_number_name(heater, 'Name')
                
                conversion_sel = selection.Selection()
                
                @conversion_sel.option('conversion')
                def option(conversion_config):
                    gen.add_aprinter_include('printer/thermistor/GenericThermistor.h')
                    return TemplateExpr('GenericThermistorService', [
                        gen.add_float_config('{}HeaterTempResistorR'.format(name), conversion_config.get_float('ResistorR')),
                        gen.add_float_config('{}HeaterTempR0'.format(name), conversion_config.get_float('R0')),
                        gen.add_float_config('{}HeaterTempBeta'.format(name), conversion_config.get_float('Beta')),
                        gen.add_float_config('{}HeaterTempMinTemp'.format(name), conversion_config.get_float('MinTemp')),
                        gen.add_float_config('{}HeaterTempMaxTemp'.format(name), conversion_config.get_float('MaxTemp')),
                    ])
                
                @conversion_sel.option('Max31855Formula')
                def option(conversion_config):
                    gen.add_aprinter_include('printer/thermistor/Max31855Formula.h')
                    return 'Max31855FormulaService'
                
                conversion = heater.do_selection('conversion', conversion_sel)
                
                for control in heater.enter_config('control'):
                    gen.add_aprinter_include('printer/temp_control/PidControl.h')
                    control_interval = control.get_float('ControlInterval')
                    control_service = TemplateExpr('PidControlService', [
                        gen.add_float_config('{}HeaterPidP'.format(name), control.get_float('PidP')),
                        gen.add_float_config('{}HeaterPidI'.format(name), control.get_float('PidI')),
                        gen.add_float_config('{}HeaterPidD'.format(name), control.get_float('PidD')),
                        gen.add_float_config('{}HeaterPidIStateMin'.format(name), control.get_float('PidIStateMin')),
                        gen.add_float_config('{}HeaterPidIStateMax'.format(name), control.get_float('PidIStateMax')),
                        gen.add_float_config('{}HeaterPidDHistory'.format(name), control.get_float('PidDHistory')),
                    ])
                
                for observer in heater.enter_config('observer'):
                    gen.add_aprinter_include('printer/TemperatureObserver.h')
                    observer_service = TemplateExpr('TemperatureObserverService', [
                        gen.add_float_config('{}HeaterObserverInterval'.format(name), observer.get_float('ObserverInterval')),
                        gen.add_float_config('{}HeaterObserverTolerance'.format(name), observer.get_float('ObserverTolerance')),
                        gen.add_float_config('{}HeaterObserverMinTime'.format(name), observer.get_float('ObserverMinTime')),
                    ])
                
                return TemplateExpr('AuxControlModuleHeaterParams', [
                    name_expr,
                    heater.get_int('SetMCommand'),
                    use_analog_input(gen, heater, 'ThermistorInput', '{}::GetHeaterAnalogInput<{}>'.format(aux_control_module_user, heater_index)),
                    conversion,
                    gen.add_float_config('{}HeaterMinSafeTemp'.format(name), heater.get_float('MinSafeTemp')),
                    gen.add_float_config('{}HeaterMaxSafeTemp'.format(name), heater.get_float('MaxSafeTemp')),
                    gen.add_float_config('{}HeaterControlInterval'.format(name), control_interval),
                    control_service,
                    observer_service,
                    use_pwm_output(gen, heater, 'pwm_output', '{}::GetHeaterPwm<{}>'.format(aux_control_module_user, heater_index), '{}Heater'.format(name))
                ])
            
            heaters_expr = config.do_list('heaters', heater_cb, max_count=15)
            
            transform_sel = selection.Selection()
            transform_axes = []
            
            @transform_sel.option('NoTransform')
            def option(transform):
                return 'PrinterMainNoTransformParams'
            
            @transform_sel.default()
            def default(transform_type, transform):
                def virtual_axis_cb(virtual_axis, virtual_axis_index):
                    name = virtual_axis.get_id_char('Name')
                    transform_axes.append(name)
                    
                    homing_sel = selection.Selection()
                    
                    @homing_sel.option('no_homing')
                    def option(homing):
                        return 'PrinterMainNoVirtualHomingParams'
                    
                    @homing_sel.option('homing')
                    def option(homing):
                        return TemplateExpr('PrinterMainVirtualHomingParams', [
                            use_digital_input(gen, homing, 'HomeEndstopInput'),
                            gen.add_bool_config('{}HomeEndInvert'.format(name), homing.get_bool('HomeEndInvert')),
                            gen.add_bool_config('{}HomeDir'.format(name), homing.get_bool('HomeDir')),
                            gen.add_float_config('{}HomeFastExtraDist'.format(name), homing.get_float('HomeFastExtraDist')),
                            gen.add_float_config('{}HomeRetractDist'.format(name), homing.get_float('HomeRetractDist')),
                            gen.add_float_config('{}HomeSlowExtraDist'.format(name), homing.get_float('HomeSlowExtraDist')),
                            gen.add_float_config('{}HomeFastSpeed'.format(name), homing.get_float('HomeFastSpeed')),
                            gen.add_float_config('{}HomeRetractSpeed'.format(name), homing.get_float('HomeRetractSpeed')),
                            gen.add_float_config('{}HomeSlowSpeed'.format(name), homing.get_float('HomeSlowSpeed')),
                        ])
                    
                    return TemplateExpr('PrinterMainVirtualAxisParams', [
                        TemplateChar(name),
                        gen.add_float_config('{}MinPos'.format(name), virtual_axis.get_float('MinPos')),
                        gen.add_float_config('{}MaxPos'.format(name), virtual_axis.get_float('MaxPos')),
                        gen.add_float_config('{}MaxSpeed'.format(name), virtual_axis.get_float('MaxSpeed')),
                        virtual_axis.do_selection('homing', homing_sel),
                    ])
                
                def transform_stepper_cb(transform_stepper, transform_stepper_index):
                    stepper_name = transform_stepper.get_id_char('StepperName')
                    try:
                        stepper_generator = config.enter_elem_by_id('steppers', 'Name', stepper_name)
                    except config_reader.ConfigError:
                        transform_stepper.path().error('Unknown stepper \'{}\' referenced.'.format(stepper_name))
                    
                    for stepper in stepper_generator:
                        if stepper.get_bool('EnableCartesianSpeedLimit'):
                            stepper.key_path('EnableCartesianSpeedLimit').error('Stepper involved coordinate transform may not be cartesian.')
                    
                    return TemplateExpr('WrapInt', [TemplateChar(stepper_name)])
                
                transform_type_sel = selection.Selection()
                
                def distance_splitter(prefix):
                    gen.add_aprinter_include('printer/transform/DistanceSplitter.h')
                    return TemplateExpr('DistanceSplitterService', [
                        gen.add_float_config('{}MinSplitLength'.format(prefix), transform.get_float('MinSplitLength')),
                        gen.add_float_config('{}MaxSplitLength'.format(prefix), transform.get_float('MaxSplitLength')),
                        gen.add_float_config('{}SegmentsPerSecond'.format(prefix), transform.get_float('SegmentsPerSecond')),
                    ])
                
                @transform_type_sel.option('Null')
                def option():
                    gen.add_aprinter_include('printer/transform/IdentityTransform.h')
                    gen.add_aprinter_include('printer/transform/NoSplitter.h')
                    
                    return TemplateExpr('IdentityTransformService', [0]), 'NoSplitterService'
                
                @transform_type_sel.option('CoreXY')
                def option():
                    gen.add_aprinter_include('printer/transform/CoreXyTransform.h')
                    gen.add_aprinter_include('printer/transform/NoSplitter.h')
                    
                    return 'CoreXyTransformService', 'NoSplitterService'
                
                @transform_type_sel.option('Delta')
                def option():
                    gen.add_aprinter_include('printer/transform/DeltaTransform.h')
                    
                    return TemplateExpr('DeltaTransformService', [
                        gen.add_float_config('DeltaDiagonalRod', transform.get_float('DiagnalRod')),
                        gen.add_float_config('DeltaSmoothRodOffset', transform.get_float('SmoothRodOffset')),
                        gen.add_float_config('DeltaEffectorOffset', transform.get_float('EffectorOffset')),
                        gen.add_float_config('DeltaCarriageOffset', transform.get_float('CarriageOffset')),
                        gen.add_float_config('DeltaLimitRadius', transform.get_float('LimitRadius')),
                    ]), distance_splitter('Delta')
                
                @transform_type_sel.option('RotationalDelta')
                def option():
                    gen.add_aprinter_include('printer/transform/RotationalDeltaTransform.h')
                    
                    return TemplateExpr('RotationalDeltaTransformService', [
                        gen.add_float_config('DeltaEndEffectorLength', transform.get_float('EndEffectorLength')),
                        gen.add_float_config('DeltaBaseLength', transform.get_float('BaseLength')),
                        gen.add_float_config('DeltaRodLength', transform.get_float('RodLength')),
                        gen.add_float_config('DeltaArmLength', transform.get_float('ArmLength')),
                        gen.add_float_config('DeltaZOffset', transform.get_float('ZOffset')),
                    ]), distance_splitter('Delta')
                
                transform_type_expr, splitter_expr = transform_type_sel.run(transform_type)
                
                max_dimensions = 10
                
                dimension_count = transform.get_int('DimensionCount')
                if not 0 <= dimension_count <= max_dimensions:
                    transform.path().error('Incorrect DimensionCount.')
                
                virtual_axes = transform.do_keyed_list(dimension_count, 'CartesianAxes', 'VirtualAxis', virtual_axis_cb)
                transform_steppers = transform.do_keyed_list(dimension_count, 'Steppers', 'TransformStepper', transform_stepper_cb)
                
                if transform.has('IdentityAxes'):
                    num_idaxes = 0
                    for idaxis in transform.iter_list_config('IdentityAxes', max_count=max_dimensions-dimension_count):
                        virt_axis_name = idaxis.get_id_char('Name')
                        stepper_name = idaxis.get_id_char('StepperName')
                        transform_axes.append(virt_axis_name)
                        num_idaxes += 1
                        virtual_axes.append_arg(TemplateExpr('PrinterMainVirtualAxisParams', [
                            TemplateChar(virt_axis_name),
                            gen.add_float_config('{}MinPos'.format(virt_axis_name), float('-inf'), is_constant=True),
                            gen.add_float_config('{}MaxPos'.format(virt_axis_name), float('+inf'), is_constant=True),
                            gen.add_float_config('{}MaxSpeed'.format(virt_axis_name), float('inf'), is_constant=True),
                            'PrinterMainNoVirtualHomingParams',
                        ]))
                        transform_steppers.append_arg(TemplateExpr('WrapInt', [TemplateChar(stepper_name)]))
                    
                    if num_idaxes > 0:
                        gen.add_aprinter_include('printer/transform/IdentityTransform.h')
                        id_transform_type_expr = TemplateExpr('IdentityTransformService', [num_idaxes])
                        
                        if dimension_count == 0:
                            transform_type_expr = id_transform_type_expr
                        else:
                            gen.add_aprinter_include('printer/transform/CombineTransform.h')
                            transform_type_expr = TemplateExpr('CombineTransformService', [transform_type_expr, id_transform_type_expr])
                        
                        dimension_count += num_idaxes
                
                if dimension_count == 0:
                    transform.path().error('Need at least one dimension.')
                
                return TemplateExpr('PrinterMainTransformParams', [
                    virtual_axes,
                    transform_steppers,
                    transform_type_expr,
                    splitter_expr,
                ])
            
            transform_expr = config.do_selection('transform', transform_sel)
            
            probe_sel = selection.Selection()
            
            @probe_sel.option('NoProbe')
            def option(probe):
                pass
            
            @probe_sel.option('Probe')
            def option(probe):
                gen.add_aprinter_include('printer/BedProbeModule.h')
                
                probe_module = gen.add_module()
                
                gen.add_bool_config('ProbeInvert', probe.get_bool('InvertInput')),
                gen.add_float_config('ProbeOffsetX', probe.get_float('OffsetX'))
                gen.add_float_config('ProbeOffsetY', probe.get_float('OffsetY'))
                gen.add_float_config('ProbeStartHeight', probe.get_float('StartHeight'))
                gen.add_float_config('ProbeLowHeight', probe.get_float('LowHeight'))
                gen.add_float_config('ProbeRetractDist', probe.get_float('RetractDist'))
                gen.add_float_config('ProbeMoveSpeed', probe.get_float('MoveSpeed'))
                gen.add_float_config('ProbeFastSpeed', probe.get_float('FastSpeed'))
                gen.add_float_config('ProbeRetractSpeed', probe.get_float('RetractSpeed'))
                gen.add_float_config('ProbeSlowSpeed', probe.get_float('SlowSpeed'))
                gen.add_float_config('ProbeGeneralZOffset', probe.get_float('GeneralZOffset'))
                
                num_points = 0
                for (i, point) in enumerate(probe.iter_list_config('ProbePoints', min_count=1, max_count=20)):
                    num_points += 1
                    gen.add_bool_config('ProbeP{}Enabled'.format(i+1), point.get_bool('Enabled'))
                    gen.add_float_config('ProbeP{}X'.format(i+1), point.get_float('X'))
                    gen.add_float_config('ProbeP{}Y'.format(i+1), point.get_float('Y'))
                    gen.add_float_config('ProbeP{}ZOffset'.format(i+1), point.get_float('Z-offset'))
                
                correction_sel = selection.Selection()
                
                @correction_sel.option('NoCorrection')
                def option(correction):
                    return 'BedProbeNoCorrectionParams'
                
                @correction_sel.option('Correction')
                def option(correction):
                    if 'Z' not in transform_axes:
                        correction.path().error('Bed correction is only supported when the Z axis is involved in the coordinate transformation.')
                    
                    quadratic_supported = correction.get_bool('QuadraticCorrectionSupported')
                    quadratic_enabled = gen.add_bool_config('ProbeQuadrCorrEnabled', correction.get_bool('QuadraticCorrectionEnabled')) if quadratic_supported else 'void'
                    
                    return TemplateExpr('BedProbeCorrectionParams', [quadratic_supported, quadratic_enabled])
                
                correction_expr = probe.do_selection('correction', correction_sel)
                
                probe_module.set_expr(TemplateExpr('BedProbeModuleService', [
                    'MakeTypeList<WrapInt<\'X\'>, WrapInt<\'Y\'>>',
                    '\'Z\'',
                    use_digital_input(gen, probe, 'ProbePin'),
                    'ProbeInvert',
                    'MakeTypeList<ProbeOffsetX, ProbeOffsetY>',
                    'ProbeStartHeight',
                    'ProbeLowHeight',
                    'ProbeRetractDist',
                    'ProbeMoveSpeed',
                    'ProbeFastSpeed',
                    'ProbeRetractSpeed',
                    'ProbeSlowSpeed',
                    'ProbeGeneralZOffset',
                    TemplateList(['BedProbePointParams<ProbeP{0}Enabled, MakeTypeList<ProbeP{0}X, ProbeP{0}Y>, ProbeP{0}ZOffset>'.format(i+1) for i in range(num_points)]),
                    correction_expr,
                ]))
            
            config.get_config('probe_config').do_selection('probe', probe_sel)
            
            def fan_cb(fan, fan_index):
                name, name_expr = get_letter_number_name(fan, 'Name')
                
                return TemplateExpr('AuxControlModuleFanParams', [
                    name_expr,
                    fan.get_int('SetMCommand'),
                    fan.get_int('OffMCommand'),
                    'FanSpeedMultiply',
                    use_pwm_output(gen, fan, 'pwm_output', '{}::GetFanPwm<{}>'.format(aux_control_module_user, fan_index), '{}Fan'.format(name))
                ])
            
            fans_expr = config.do_list('fans', fan_cb, max_count=15)
            
            def laser_cb(laser, laser_index):
                gen.add_aprinter_include('driver/LaserDriver.h')
                gen.add_aprinter_include('printer/duty_formula/LinearDutyFormula.h')
                
                name = laser.get_id_char('Name')
                laser_port = gen.get_object('laser_port', laser, 'laser_port')
                
                return TemplateExpr('PrinterMainLaserParams', [
                    TemplateChar(name),
                    TemplateChar(laser.get_id_char('DensityName')),
                    gen.add_float_config('{}LaserPower'.format(name), laser.get_float('LaserPower')),
                    gen.add_float_config('{}MaxPower'.format(name), laser.get_float('MaxPower')),
                    use_pwm_output(gen, laser_port, 'pwm_output', '', '', hard=True),
                    TemplateExpr('LinearDutyFormulaService', [15]),
                    TemplateExpr('LaserDriverService', [
                        use_interrupt_timer(gen, laser_port, 'LaserTimer', user='MyPrinter::GetLaserDriver<{}>::TheTimer'.format(laser_index)),
                        gen.add_float_constant('{}AdjustmentInterval'.format(name), laser.get_float('AdjustmentInterval')),
                        'LaserDriverDefaultPrecisionParams',
                    ]),
                ])
            
            lasers_expr = config.do_list('lasers', laser_cb, max_count=15)
            
            current_sel = selection.Selection()
            
            @current_sel.option('NoCurrent')
            def option(current):
                pass
            
            @current_sel.option('Current')
            def option(current):
                gen.add_aprinter_include('printer/MotorCurrentModule.h')
                current_module = gen.add_module()
                current_module.set_expr(TemplateExpr('MotorCurrentModuleService', [
                    TemplateList(current_control_channel_list),
                    use_current_driver(gen, current, 'current_driver', 'MyPrinter::GetModule<{}>::GetDriver'.format(current_module.index))
                ]))
            
            current_config.do_selection('current', current_sel)
            
            gen.add_aprinter_include('printer/AuxControlModule.h')
            aux_control_module.set_expr(TemplateExpr('AuxControlModuleService', [
                performance.get_int_constant('EventChannelBufferSize'),
                event_channel_timer_expr,
                gen.add_float_config('WaitTimeout', config.get_float('WaitTimeout')),
                gen.add_float_config('WaitReportPeriod', config.get_float('WaitReportPeriod')),
                heaters_expr,
                fans_expr,
            ]))
            
            printer_params = TemplateExpr('PrinterMainParams', [
                led_pin_expr,
                'LedBlinkInterval',
                gen.add_float_config('InactiveTime', config.get_float('InactiveTime')),
                gen.add_float_constant('SpeedLimitMultiply', 1.0 / 60.0),
                gen.add_float_config('MaxStepsPerCycle', performance.get_float('MaxStepsPerCycle')),
                performance.get_int_constant('StepperSegmentBufferSize'),
                performance.get_int_constant('LookaheadBufferSize'),
                performance.get_int_constant('LookaheadCommitCount'),
                'ForceTimeout',
                performance.get_identifier('FpType', lambda x: x in ('float', 'double')),
                setup_watchdog(gen, platform, 'watchdog', disable_watchdog, 'MyPrinter::GetWatchdog'),
                config_manager_expr,
                'ConfigList',
                steppers_expr,
                transform_expr,
                lasers_expr,
                TemplateList(gen.get_modules_exprs()),
            ])
            
            printer_params_typedef = 'struct ThePrinterParams : public {} {{}};'.format(printer_params.build(0))
            
            gen.add_global_resource(30, 'MyPrinter', TemplateExpr('PrinterMain', ['MyContext', 'Program', 'ThePrinterParams']), context_name='Printer', code_before=printer_params_typedef)
            gen.add_subst('FastEventRoot', 'MyPrinter')
            gen.add_subst('EmergencyProvider', 'MyPrinter')
    
    gen.finalize()
    
    return {
        'main_source': rich_template.RichTemplate(main_template).substitute(gen.get_subst()),
        'board_for_build': board_for_build,
        'output_type': output_type,
        'optimize_for_size': optimize_for_size,
        'assertions_enabled': assertions_enabled,
        'event_loop_benchmark_enabled': event_loop_benchmark_enabled,
        'detect_overload_enabled': detect_overload_enabled,
        'build_with_clang': build_with_clang,
        'verbose_build': verbose_build,
        'debug_symbols': debug_symbols,
    }

def main():
    # Parse arguments.
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--config', help='JSON configuration file to use.')
    parser.add_argument('--cfg-name', help='Build this configuration instead of the one specified in the configuration file.')
    parser.add_argument('--output', default='-', help='File to write the output to (C++ code or Nix expression).')
    args = parser.parse_args()
    
    # Determine directories.
    src_dir = file_utils.file_dir(__file__)
    nix_dir = os.path.join(src_dir, '..', '..', 'nix')
    
    # Read the configuration.
    config = args.config if args.config is not None else os.path.join(src_dir, '..', 'gui', 'default_config.json')
    with file_utils.use_input_file(config) as config_f:
        config_data = json.load(config_f)
    
    # Read main template file.
    main_template = file_utils.read_file(os.path.join(src_dir, 'main_template.cpp'))
    
    # Call the generate function.
    result = generate(config_data, args.cfg_name, main_template)
    
    # Build the Nix expression.
    nix_expr = (
        'with ((import (builtins.toPath {})) {{}}); aprinterFunc {{\n'
        '    boardName = {}; buildName = "aprinter"; desiredOutputs = [{}]; optimizeForSize = {};\n'
        '    assertionsEnabled = {}; eventLoopBenchmarkEnabled = {}; detectOverloadEnabled = {};\n'
        '    buildWithClang = {}; verboseBuild = {}; debugSymbols = {}; mainText = {};\n'
        '}}\n'
    ).format(
        nix_utils.escape_string_for_nix(nix_dir),
        nix_utils.escape_string_for_nix(result['board_for_build']),
        nix_utils.escape_string_for_nix(result['output_type']),
        nix_utils.convert_bool_for_nix(result['optimize_for_size']),
        nix_utils.convert_bool_for_nix(result['assertions_enabled']),
        nix_utils.convert_bool_for_nix(result['event_loop_benchmark_enabled']),
        nix_utils.convert_bool_for_nix(result['detect_overload_enabled']),
        nix_utils.convert_bool_for_nix(result['build_with_clang']),
        nix_utils.convert_bool_for_nix(result['verbose_build']),
        nix_utils.convert_bool_for_nix(result['debug_symbols']),
        nix_utils.escape_string_for_nix(result['main_source'])
    )
    
    # Write output.
    with file_utils.use_output_file(args.output) as output_f:
        output_f.write(nix_expr)

main()
