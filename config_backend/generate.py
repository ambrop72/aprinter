from __future__ import print_function
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../config_common'))
import argparse
import json
import re
import config_common
import config_reader

class GenState(object):
    def __init__ (self):
        self._subst = {}
        self._config_options = []
        self._platform_includes = []
        self._aprinter_includes = []
        self._isrs = []
        self._clock = None
    
    def add_subst (self, key, val):
        self._subst[key] = val
    
    def add_config (self, name, dtype, value):
        self._config_options.append({'name':name, 'dtype':dtype, 'value':value})
    
    def add_float_config (self, name, value):
        self.add_config(name, 'DOUBLE', '{:.17E}'.format(value))
    
    def add_bool_config (self, name, value):
        self.add_config(name, 'BOOL', 'true' if value else 'false')
    
    def add_platform_include (self, inc_file):
        self._platform_includes.append(inc_file)
    
    def add_aprinter_include (self, inc_file):
        self._aprinter_includes.append(inc_file)
    
    def add_isr (self, isr):
        self._isrs.append(isr)
    
    def set_clock (self, clock):
        self._clock = clock
    
    def add_interrupt_timer (self, config, user, clearance=None):
        for it_config in config:
            return self._clock.add_interrupt_timer(it_config.get_string('oc_unit'), user, clearance, it_config.path())
    
    def add_automatic (self):
        self._clock.add_automatic()
        
        self.add_subst('EXTRA_CONFIG', ''.join('APRINTER_CONFIG_OPTION_{}({}, {}, ConfigNoProperties)\n'.format(c['dtype'], c['name'], c['value']) for c in self._config_options))
        self.add_subst('PLATFORM_INCLUDES', ''.join('#include <{}>\n'.format(inc) for inc in self._platform_includes))
        self.add_subst('EXTRA_APRINTER_INCLUDES', ''.join('#include <aprinter/{}>\n'.format(inc) for inc in self._aprinter_includes))
        self.add_subst('ISRS', ''.join('{}\n'.format(isr) for isr in self._isrs))
    
    def get_subst (self):
        return self._subst

class GenConfigReader(config_reader.ConfigReader):
    def __init__ (self, obj, path):
        config_reader.ConfigReader.__init__(self, obj, path)
    
    def config_factory (self, obj, path):
        return GenConfigReader(obj, path)
    
    def get_int_constant (self, key):
        return str(self.get_int(key))

    def get_float_constant (self, key):
        return '{:.17E}'.format(self.get_float(key))

    def get_identifier (self, key, validate=None):
        val = self.get_string(key)
        if not re.match('\\A[A-Za-z][A-Za-z0-9]{0,127}\\Z', val):
            self.key_path(key).error('Incorrect format.')
        if validate is not None and not validate(val):
            self.key_path(key).error('Custom validation failed.')
        return val
    
    def get_pin (self, key):
        return self.get_identifier(key)
    
    def do_selection (self, key, sel_def):
        for config in self.enter_config(key):
            try:
                result = sel_def.run(config.get_string('_compoundName'), config)
            except config_common.SelectionError:
                config.path().error('Unknown choice.')
            return result

class At91Sam3xClock(object):
    def __init__ (self, gen, config):
        self._gen = gen
        gen.add_aprinter_include('system/At91Sam3xClock.h')
        gen.add_subst('CLOCK_CONFIG', 'static const int clock_timer_prescaler = {};'.format(config.get_int_constant('prescaler')))
        gen.add_subst('CLOCK', 'At91Sam3xClock<MyContext, Program, clock_timer_prescaler, ClockTcsList>')
        self._primary_timer = config.get_identifier('primary_timer', lambda x: re.match('\\ATC[0-9]\\Z', x))
        self._interrupt_timers = []
    
    def add_interrupt_timer (self, name, user, clearance, path):
        m = re.match('\\A(TC[0-9])([A-C])\\Z', name)
        if m is None:
            path.error('Incorrect OC unit format.')
        it = {'tc':m.group(1), 'channel':m.group(2)}
        self._interrupt_timers.append(it)
        clearance_extra = ', {}'.format(clearance) if clearance is not None else ''
        self._gen.add_isr('AMBRO_AT91SAM3X_CLOCK_INTERRUPT_TIMER_GLOBAL(At91Sam3xClock{}, At91Sam3xClockComp{}, {}, MyContext())'.format(it['tc'], it['channel'], user))
        return 'At91Sam3xClockInterruptTimerService<At91Sam3xClock{}, At91Sam3xClockComp{}{}>'.format(it['tc'], it['channel'], clearance_extra)
    
    def add_automatic (self):
        tcs = set(it['tc'] for it in self._interrupt_timers)
        tcs.add(self._primary_timer)
        tcs = sorted(tcs)
        self._gen.add_subst('CLOCK_TCS', 'using ClockTcsList = MakeTypeList<{}>;'.format(', '.join('At91Sam3xClock{}'.format(tc) for tc in tcs)))
        for tc in tcs:
            self._gen.add_isr('AMBRO_AT91SAM3X_CLOCK_{}_GLOBAL(MyClock, MyContext())\n'.format(tc))

def generate(config_root_data, cfg_name, main_template):
    gen = GenState()
    
    for config_root in config_reader.start(config_root_data, config_reader_class=GenConfigReader):
        config_root.mark('_compoundName')
        config_root.mark('boards')
        
        for config in config_root.enter_elem_by_id('configurations', 'name', cfg_name):
            for board_data in config.enter_config('board_data'):
                for platform in board_data.enter_config('platform'):
                    clock_sel = config_common.Selection()
                    
                    @clock_sel.option('At91Sam3xClock')
                    def option(clock):
                        return At91Sam3xClock(gen, clock)
                    
                    gen.set_clock(platform.do_selection('clock', clock_sel))
                
                gen.add_subst('LedPin', board_data.get_identifier('LedPin'))
                gen.add_subst('EventChannelTimer', gen.add_interrupt_timer(board_data.enter_config('EventChannelTimer'), user='MyPrinter::GetEventChannelTimer', clearance='EventChannelTimerClearance'))
                
                for performance in board_data.enter_config('performance'):
                    gen.add_subst('AxisDriverPrecisionParams', performance.get_identifier('AxisDriverPrecisionParams'))
                    gen.add_subst('EventChannelTimerClearance', performance.get_float_constant('EventChannelTimerClearance'))
                    gen.add_float_config('MaxStepsPerCycle', performance.get_float('MaxStepsPerCycle'))
                    gen.add_subst('StepperSegmentBufferSize', performance.get_int_constant('StepperSegmentBufferSize'))
                    gen.add_subst('EventChannelBufferSize', performance.get_int_constant('EventChannelBufferSize'))
                    gen.add_subst('LookaheadBufferSize', performance.get_int_constant('LookaheadBufferSize'))
                    gen.add_subst('LookaheadCommitCount', performance.get_int_constant('LookaheadCommitCount'))
                    gen.add_subst('FpType', performance.get_identifier('FpType', lambda x: x in ('float', 'double')))
                
                for serial in board_data.enter_config('serial'):
                    gen.add_subst('SerialBaudRate', serial.get_int_constant('BaudRate'))
                    gen.add_subst('SerialRecvBufferSizeExp', serial.get_int_constant('RecvBufferSizeExp'))
                    gen.add_subst('SerialSendBufferSizeExp', serial.get_int_constant('SendBufferSizeExp'))
                    gen.add_subst('SerialGcodeMaxParts', serial.get_int_constant('GcodeMaxParts'))
                    
                    serial_sel = config_common.Selection()
                    
                    @serial_sel.option('AsfUsbSerial')
                    def option(serial_service):
                        return 'AsfUsbSerialService'
                    
                    @serial_sel.option('At91Sam3xSerial')
                    def option(serial_service):
                        return 'At91Sam3xSerialService'
                    
                    gen.add_subst('SerialService', serial.do_selection('Service', serial_sel))
                
                sdcard_sel = config_common.Selection()
                
                @sdcard_sel.option('NoSdCard')
                def option(sdcard):
                    return 'PrinterMainNoSdCardParams'
                
                @sdcard_sel.option('SdCard')
                def option(sdcard):
                    sd_service_sel = config_common.Selection()
                    
                    @sd_service_sel.option('SpiSdCard')
                    def option(spi_sd):
                        return 'SpiSdCardService<{}, {}>'.format(spi_sd.get_pin('SsPin'), 'TBD')
                    
                    return 'PrinterMainSdCardParams<{}, {}, {}, {}>'.format(sdcard.do_selection('SdCardService', sd_service_sel), 'TBD, TBD', sdcard.get_int('BufferBaseSize'), sdcard.get_int('MaxCommandSize'))
                
                gen.add_subst('SdCard', board_data.do_selection('sdcard', sdcard_sel))
                
            
            gen.add_float_config('InactiveTime', config.get_float('InactiveTime'))
            
            for advanced in config.enter_config('advanced'):
                gen.add_subst('LedBlinkInterval', advanced.get_float_constant('LedBlinkInterval'))
                gen.add_float_config('ForceTimeout', advanced.get_float('ForceTimeout'))
    
    gen.add_automatic()
    
    print(gen.get_subst())
    
    main_text = config_common.RichTemplate(main_template).substitute(gen.get_subst())
    print(main_text)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True, type=argparse.FileType('r'))
    parser.add_argument('--cfg-name', required=True)
    args = parser.parse_args()
    
    # Determine source dir.
    src_dir = config_common.file_dir(__file__)
    
    # Read main template file.
    main_template = config_common.read_file(os.path.join(src_dir, 'main_template.cpp'))
    
    # Generate.
    generate(json.load(args.config), args.cfg_name, main_template)

main()
