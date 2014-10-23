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
        self._constants = []
        self._platform_includes = []
        self._aprinter_includes = []
        self._isrs = []
        self._clock = None
        self._digital_inputs = {}
    
    def add_subst (self, key, val):
        self._subst[key] = val
    
    def add_config (self, name, dtype, value):
        self._config_options.append({'name':name, 'dtype':dtype, 'value':value})
        return name
    
    def add_float_config (self, name, value):
        return self.add_config(name, 'DOUBLE', '{:.17E}'.format(value))
    
    def add_bool_config (self, name, value):
        return self.add_config(name, 'BOOL', 'true' if value else 'false')
    
    def add_float_constant (self, config, key, name):
        value = config.get_float(key)
        self._constants.append({'name':name, 'value':'AMBRO_WRAP_DOUBLE({})'.format(value)})
        return name
    
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
    
    def register_digital_input (self, di_config):
        name = di_config.get_string('Name')
        if name in self._digital_inputs:
            di_config.path().error('Duplicate digital input name')
        self._digital_inputs[name] = {'Pin':di_config.get_pin('Pin'), 'InputMode':di_config.get_identifier('InputMode')}
    
    def use_digital_input (self, config, key):
        name = config.get_string(key)
        if name not in self._digital_inputs:
            config.key_path(key).error('Nonexistent digital input specified')
        di = self._digital_inputs[name]
        return '{}, {}'.format(di['Pin'], di['InputMode'])
    
    def use_spi (self, config, key, user):
        spi_sel = config_common.Selection()
        
        @spi_sel.option('At91SamSpi')
        def option(spi_config):
            devices = {
                'At91Sam3xSpiDevice':'AMBRO_AT91SAM3X_SPI_GLOBAL',
                'At91Sam3uSpiDevice':'AMBRO_AT91SAM3U_SPI_GLOBAL'
            }
            dev = spi_config.get_identifier('Device')
            if dev not in devices:
                spi_config.path().error('Incorrect SPI device.')
            self.add_isr('{}({}, MyContext())'.format(devices[dev], user))
            return 'At91SamSpiService<{}>'.format(dev)
        
        return config.do_selection(key, spi_sel)
    
    def use_i2c (self, config, key, user, username):
        i2c_sel = config_common.Selection()
        
        @i2c_sel.option('At91SamI2c')
        def option(i2c_config):
            devices = {
                'At91SamI2cDevice1':1,
            }
            dev = i2c_config.get_identifier('Device')
            if dev not in devices:
                i2c_config.path().error('Incorrect I2C device.')
            self.add_isr('AMBRO_AT91SAM_I2C_GLOBAL({}, {}, MyContext())'.format(devices[dev], user))
            return 'At91SamI2cService<{}, {}, {}>'.format(dev, i2c_config.get_int('Ckdiv'), self.add_float_constant(i2c_config, 'I2cFreq', '{}I2cFreq'.format(username)))
        
        return config.do_selection(key, i2c_sel)
    
    def add_automatic (self):
        self._clock.add_automatic()
        
        self.add_subst('EXTRA_CONSTANTS', ''.join('using {} = {};\n'.format(c['name'], c['value']) for c in self._constants))
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
    
    def get_bool_constant (self, key):
        return 'true' if self.get_bool(key) else 'false'

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
                
                for digital_input in board_data.iter_list_config('digital_inputs'):
                    gen.register_digital_input(digital_input)
                
                gen.add_subst('LedPin', board_data.get_identifier('LedPin'))
                gen.add_subst('EventChannelTimer', gen.add_interrupt_timer(board_data.enter_config('EventChannelTimer'), user='MyPrinter::GetEventChannelTimer', clearance='EventChannelTimerClearance'))
                
                for performance in board_data.enter_config('performance'):
                    gen.add_subst('AxisDriverPrecisionParams', performance.get_identifier('AxisDriverPrecisionParams'))
                    gen.add_float_constant(performance, 'EventChannelTimerClearance', 'EventChannelTimerClearance')
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
                        gen.add_isr('AMBRO_AT91SAM3X_SERIAL_GLOBAL(MyPrinter::GetSerial, MyContext())')
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
                        return 'SpiSdCardService<{}, {}>'.format(spi_sd.get_pin('SsPin'), gen.use_spi(spi_sd, 'SpiService', 'MyPrinter::GetSdCard<>::GetSpi'))
                    
                    gcode_parser_sel = config_common.Selection()
                    
                    @gcode_parser_sel.option('TextGcodeParser')
                    def option(parser):
                        return 'FileGcodeParser, GcodeParserParams<{}>'.format(parser.get_int('MaxParts'))
                    
                    @gcode_parser_sel.option('BinaryGcodeParser')
                    def option(parser):
                        return 'BinaryGcodeParser, BinaryGcodeParserParams<{}>'.format(parser.get_int('MaxParts'))
                    
                    return 'PrinterMainSdCardParams<{}, {}, {}, {}>'.format(sdcard.do_selection('SdCardService', sd_service_sel), sdcard.do_selection('GcodeParser', gcode_parser_sel), sdcard.get_int('BufferBaseSize'), sdcard.get_int('MaxCommandSize'))
                
                gen.add_subst('SdCard', board_data.do_selection('sdcard', sdcard_sel))
                
                config_manager_sel = config_common.Selection()
                
                @config_manager_sel.option('ConstantConfigManager')
                def option(config_manager):
                    return 'ConstantConfigManagerService'
                
                @config_manager_sel.option('RuntimeConfigManager')
                def option(config_manager):
                    config_store_sel = config_common.Selection()
                    
                    @config_store_sel.option('NoStore')
                    def option(config_store):
                        return 'RuntimeConfigManagerNoStoreService'
                    
                    @config_store_sel.option('EepromConfigStore')
                    def option(config_store):
                        eeprom_sel = config_common.Selection()
                        
                        @eeprom_sel.option('I2cEeprom')
                        def option(eeprom):
                            return 'I2cEepromService<{}, {}, {}, {}, {}>'.format(gen.use_i2c(eeprom, 'I2c', 'MyPrinter::GetConfigManager::GetStore<>::GetEeprom::GetI2', 'ConfigEeprom'), eeprom.get_int('I2cAddr'), eeprom.get_int('Size'), eeprom.get_int('BlockSize'), gen.add_float_config('ConfigEepromWriteTimeout', eeprom.get_float('WriteTimeout')))
                        
                        return 'EepromConfigStoreService<{}, {}, {}>'.format(config_store.do_selection('Eeprom', eeprom_sel), config_store.get_int('StartBlock'), config_store.get_int('EndBlock'))
                    
                    return 'RuntimeConfigManagerService<{}>'.format(config_manager.do_selection('ConfigStore', config_store_sel))
                
                gen.add_subst('ConfigManager', board_data.do_selection('config_manager', config_manager_sel))
            
            gen.add_float_config('InactiveTime', config.get_float('InactiveTime'))
            
            for advanced in config.enter_config('advanced'):
                gen.add_float_constant(advanced, 'LedBlinkInterval', 'LedBlinkInterval')
                gen.add_float_config('ForceTimeout', advanced.get_float('ForceTimeout'))
            
            probe_sel = config_common.Selection()
            
            @probe_sel.option('NoProbe')
            def option(probe):
                return 'PrinterMainNoProbeParams'
            
            @probe_sel.option('Probe')
            def option(probe):
                gen.add_float_config('ProbeOffsetX', probe.get_float('OffsetX'))
                gen.add_float_config('ProbeOffsetY', probe.get_float('OffsetY'))
                gen.add_float_config('ProbeStartHeight', probe.get_float('StartHeight'))
                gen.add_float_config('ProbeLowHeight', probe.get_float('LowHeight'))
                gen.add_float_config('ProbeRetractDist', probe.get_float('RetractDist'))
                gen.add_float_config('ProbeMoveSpeed', probe.get_float('MoveSpeed'))
                gen.add_float_config('ProbeFastSpeed', probe.get_float('FastSpeed'))
                gen.add_float_config('ProbeRetractSpeed', probe.get_float('RetractSpeed'))
                gen.add_float_config('ProbeSlowSpeed', probe.get_float('SlowSpeed'))
                
                point_list = []
                for (i, point) in enumerate(probe.iter_list_config('ProbePoints')):
                    p = (point.get_float('X'), point.get_float('Y'))
                    gen.add_float_config('ProbeP{}X'.format(i+1), p[0])
                    gen.add_float_config('ProbeP{}Y'.format(i+1), p[1])
                    point_list.append(p)
                
                return 'PrinterMainProbeParams<{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}>'.format(
                    'MakeTypeList<WrapInt<\'X\'>, WrapInt<\'Y\'>>',
                    '\'Z\'',
                    gen.use_digital_input(probe, 'ProbePin'),
                    probe.get_bool_constant('InvertInput'),
                    'MakeTypeList<ProbeOffsetX, ProbeOffsetY>',
                    'ProbeStartHeight',
                    'ProbeLowHeight',
                    'ProbeRetractDist',
                    'ProbeMoveSpeed',
                    'ProbeFastSpeed',
                    'ProbeRetractSpeed',
                    'ProbeSlowSpeed',
                    'MakeTypeList<{}>'.format(', '.join('MakeTypeList<ProbeP{}X, ProbeP{}Y>'.format(i+1, i+1) for i in range(len(point_list))))
                )
            
            gen.add_subst('Probe', config.do_selection('probe', probe_sel))
    
    gen.add_automatic()
    
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
