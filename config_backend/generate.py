from __future__ import print_function
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../config_common'))
import argparse
import json
import re
import string
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
        self._objects = {}
        self._singleton_objects = {}
    
    def add_subst (self, key, val, indent=-1):
        self._subst[key] = {'val':val, 'indent':indent}
    
    def add_config (self, name, dtype, value):
        self._config_options.append({'name':name, 'dtype':dtype, 'value':value})
        return name
    
    def add_float_config (self, name, value):
        return self.add_config(name, 'DOUBLE', '{:.17E}'.format(value))
    
    def add_bool_config (self, name, value):
        return self.add_config(name, 'BOOL', 'true' if value else 'false')
    
    def add_float_constant (self, name, value):
        self._constants.append({'type':'using', 'name':name, 'value':'AMBRO_WRAP_DOUBLE({:.17E})'.format(value)})
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
        self._aprinter_includes.append(inc_file)
    
    def add_isr (self, isr):
        self._isrs.append(isr)
    
    def register_objects (self, kind, config, key):
        if kind not in self._objects:
            self._objects[kind] = {}
        for obj_config in config.iter_list_config(key):
            name = obj_config.get_string('Name')
            if name in self._objects[kind]:
                obj_config.path().error('Duplicate {} name'.format(kind))
            self._objects[kind][name] = obj_config
    
    def get_object (self, kind, config, key):
        name = config.get_string(key)
        if kind not in self._objects or name not in self._objects[kind]:
            config.key_path(key).error('Nonexistent {} specified'.format(kind))
        return self._objects[kind][name]
    
    def register_singleton_object (self, kind, value, path):
        if kind in self._singleton_objects:
            path.error('Duplicate {} singleton.'.format(kind))
        self._singleton_objects[kind] = value
        return value
    
    def get_singleton_object (self, kind, path):
        if kind not in self._singleton_objects:
            path.error('Unavailable {} singleton'.format(kind))
        return self._singleton_objects[kind]
    
    def add_automatic (self):
        for so in self._singleton_objects.itervalues():
            so.add_automatic()
        
        self.add_subst('GENERATED_WARNING', 'WARNING: This file was automatically generated!')
        self.add_subst('EXTRA_CONSTANTS', ''.join('{} {} = {};\n'.format(c['type'], c['name'], c['value']) for c in self._constants))
        self.add_subst('EXTRA_CONFIG', ''.join('APRINTER_CONFIG_OPTION_{}({}, {}, ConfigNoProperties)\n'.format(c['dtype'], c['name'], c['value']) for c in self._config_options))
        self.add_subst('PLATFORM_INCLUDES', ''.join('#include <{}>\n'.format(inc) for inc in self._platform_includes))
        self.add_subst('EXTRA_APRINTER_INCLUDES', ''.join('#include <aprinter/{}>\n'.format(inc) for inc in self._aprinter_includes))
        self.add_subst('ISRS', ''.join('{}\n'.format(isr) for isr in self._isrs))
    
    def get_subst (self):
        res = {}
        for (key, subst) in self._subst.iteritems():
            val = subst['val']
            indent = subst['indent']
            res[key] = val if type(val) is str else val.build(indent)
        return res

class GenConfigReader(config_reader.ConfigReader):
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
    
    def get_id_char (self, key):
        val = self.get_string(key)
        if val not in string.ascii_uppercase:
            self.key_path(key).error('Incorrect format.')
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
    
    def do_list (self, key, elem_cb):
        elems = []
        for (i, config) in enumerate(self.iter_list_config(key)):
            elems.append(elem_cb(config, i))
        return TemplateList(elems)

class TemplateExpr(object):
    def __init__ (self, name, args):
        self._name = name
        self._args = args
    
    def build (self, indent):
        if indent == -1:
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
            self._gen.add_isr('AMBRO_AT91SAM3X_CLOCK_{}_GLOBAL(MyClock, MyContext())'.format(tc))


def setup_adc (gen, config, key):
    adc_sel = config_common.Selection()
    
    @adc_sel.option('At91SamAdc')
    def option(adc_config):
        return 'At91SamAdc<MyContext, Program, AdcPins, AdcParams>'
    
    return config.do_selection(key, adc_sel)

'''
class Adc(object):
    def __init__ (self, gen, config):
        self._config = config
        self._pins = []
    
    def add_pin (self, pin):
        self._pins.append(pin)
    
    def setup (self):
        config = self._config
        
        adc_sel = 0

class At91SamAdc(object):
    def setup (self, gen, config, pins):
        gen.add_aprinter_include('system/At91SamAdc.h')
        gen.add_float_constant('AdcFreq', config.get_float('freq'))
        gen.add_float_constant('AdcAvgInterval', config.get_float('avg_interval'))
        gen.add_int_constant('uint16', 'AdcSmoothing', int(config.get_float('smoothing') * 65536.0))
'''

def use_digital_input (gen, config, key):
    di = gen.get_object('digital_input', config, key)
    return '{}, {}'.format(di.get_pin('Pin'), di.get_identifier('InputMode'))

def use_analog_input (gen, config, key):
    ai = gen.get_object('analog_input', config, key)
    return '{}'.format(ai.get_pin('Pin'))

def use_interrupt_timer (gen, config, key, user, clearance=None):
    for it_config in config.enter_config(key):
        return gen.get_singleton_object('clock', it_config.path()).add_interrupt_timer(it_config.get_string('oc_unit'), user, clearance, it_config.path())

def use_spi (gen, config, key, user):
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
        gen.add_isr('{}({}, MyContext())'.format(devices[dev], user))
        return TemplateExpr('At91SamSpiService', [dev])
    
    return config.do_selection(key, spi_sel)

def use_i2c (gen, config, key, user, username):
    i2c_sel = config_common.Selection()
    
    @i2c_sel.option('At91SamI2c')
    def option(i2c_config):
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
                    
                    gen.register_singleton_object('clock', platform.do_selection('clock', clock_sel), platform.key_path('clock'))
                    
                    #gen.register_singleton_object('adc', 
                    
                    gen.add_subst('Adc', setup_adc(gen, platform, 'adc'))
                
                gen.register_objects('digital_input', board_data, 'digital_inputs')
                gen.register_objects('stepper_port', board_data, 'stepper_ports')
                gen.register_objects('analog_input', board_data, 'analog_inputs')
                
                gen.add_subst('LedPin', board_data.get_identifier('LedPin'))
                gen.add_subst('EventChannelTimer', use_interrupt_timer(gen, board_data, 'EventChannelTimer', user='MyPrinter::GetEventChannelTimer', clearance='EventChannelTimerClearance'))
                
                for performance in board_data.enter_config('performance'):
                    gen.add_subst('AxisDriverPrecisionParams', performance.get_identifier('AxisDriverPrecisionParams'))
                    gen.add_float_constant('EventChannelTimerClearance', performance.get_float('EventChannelTimerClearance'))
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
                        return TemplateExpr('SpiSdCardService', [spi_sd.get_pin('SsPin'), use_spi(gen, spi_sd, 'SpiService', 'MyPrinter::GetSdCard<>::GetSpi')])
                    
                    gcode_parser_sel = config_common.Selection()
                    
                    @gcode_parser_sel.option('TextGcodeParser')
                    def option(parser):
                        return 'FileGcodeParser, GcodeParserParams<{}>'.format(parser.get_int('MaxParts'))
                    
                    @gcode_parser_sel.option('BinaryGcodeParser')
                    def option(parser):
                        return 'BinaryGcodeParser, BinaryGcodeParserParams<{}>'.format(parser.get_int('MaxParts'))
                    
                    return TemplateExpr('PrinterMainSdCardParams', [sdcard.do_selection('SdCardService', sd_service_sel), sdcard.do_selection('GcodeParser', gcode_parser_sel), sdcard.get_int('BufferBaseSize'), sdcard.get_int('MaxCommandSize')])
                
                gen.add_subst('SdCard', board_data.do_selection('sdcard', sdcard_sel), indent=1)
                
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
                            return TemplateExpr('I2cEepromService', [use_i2c(gen, eeprom, 'I2c', 'MyPrinter::GetConfigManager::GetStore<>::GetEeprom::GetI2', 'ConfigEeprom'), eeprom.get_int('I2cAddr'), eeprom.get_int('Size'), eeprom.get_int('BlockSize'), gen.add_float_config('ConfigEepromWriteTimeout', eeprom.get_float('WriteTimeout'))])
                        
                        return TemplateExpr('EepromConfigStoreService', [config_store.do_selection('Eeprom', eeprom_sel), config_store.get_int('StartBlock'), config_store.get_int('EndBlock')])
                    
                    return TemplateExpr('RuntimeConfigManagerService', [config_manager.do_selection('ConfigStore', config_store_sel)])
                
                gen.add_subst('ConfigManager', board_data.do_selection('config_manager', config_manager_sel), indent=1)
            
            gen.add_float_config('InactiveTime', config.get_float('InactiveTime'))
            
            for advanced in config.enter_config('advanced'):
                gen.add_float_constant('LedBlinkInterval', advanced.get_float('LedBlinkInterval'))
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
                
                return TemplateExpr('PrinterMainProbeParams', [
                    'MakeTypeList<WrapInt<\'X\'>, WrapInt<\'Y\'>>',
                    '\'Z\'',
                    use_digital_input(gen, probe, 'ProbePin'),
                    probe.get_bool_constant('InvertInput'),
                    'MakeTypeList<ProbeOffsetX, ProbeOffsetY>',
                    'ProbeStartHeight',
                    'ProbeLowHeight',
                    'ProbeRetractDist',
                    'ProbeMoveSpeed',
                    'ProbeFastSpeed',
                    'ProbeRetractSpeed',
                    'ProbeSlowSpeed',
                    TemplateList(['MakeTypeList<ProbeP{}X, ProbeP{}Y>'.format(i+1, i+1) for i in range(len(point_list))])
                ])
            
            gen.add_subst('Probe', config.do_selection('probe', probe_sel), indent=1)
            
            def stepper_cb(stepper, stepper_index):
                name = stepper.get_id_char('Name')
                
                homing_sel = config_common.Selection()
                
                @homing_sel.option('no_homing')
                def option(homing):
                    return 'PrinterMainNoHomingParams'
                
                @homing_sel.option('homing')
                def option(homing):
                    return TemplateExpr('PrinterMainHomingParams', [
                        gen.add_bool_config('{}HomeDir'.format(name), homing.get_bool('HomeDir')),
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
                
                return TemplateExpr('PrinterMainAxisParams', [
                    TemplateChar(name),
                    stepper_port.get_pin('DirPin'),
                    stepper_port.get_pin('StepPin'),
                    stepper_port.get_pin('EnablePin'),
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
                    32,
                    TemplateExpr('AxisDriverService', [
                        use_interrupt_timer(gen, stepper_port, 'StepperTimer', user='MyPrinter::GetAxisTimer<{}>'.format(stepper_index)),
                        'TheAxisDriverPrecisionParams'
                    ]),
                    'PrinterMainNoMicroStepParams'
                ])
            
            gen.add_subst('Steppers', config.do_list('steppers', stepper_cb), indent=1)
            
            def heater_cb(heater, heater_index):
                name = heater.get_id_char('Name')
                
                for conversion in heater.enter_config('conversion'):
                    thermistor = TemplateExpr('GenericThermistorService', [
                        gen.add_float_config('{}HeaterResistorR'.format(name), conversion.get_float('ResistorR')),
                        gen.add_float_config('{}HeaterR0'.format(name), conversion.get_float('R0')),
                        gen.add_float_config('{}HeaterBeta'.format(name), conversion.get_float('Beta')),
                        gen.add_float_config('{}HeaterMinTemp'.format(name), conversion.get_float('MinTemp')),
                        gen.add_float_config('{}HeaterMaxTemp'.format(name), conversion.get_float('MaxTemp')),
                    ])
                
                for control in heater.enter_config('control'):
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
                    observer_service = TemplateExpr('TemperatureObserverService', [
                        gen.add_float_config('{}HeaterObserverInterval'.format(name), observer.get_float('ObserverInterval')),
                        gen.add_float_config('{}HeaterObserverTolerance'.format(name), observer.get_float('ObserverTolerance')),
                        gen.add_float_config('{}HeaterObserverMinTime'.format(name), observer.get_float('ObserverMinTime')),
                    ])
                
                return TemplateExpr('PrinterMainHeaterParams', [
                    TemplateChar(name),
                    heater.get_int('SetMCommand'),
                    heater.get_int('WaitMCommand'),
                    use_analog_input(gen, heater, 'ThermistorInput'),
                    thermistor,
                    gen.add_float_config('{}HeaterMinSafeTemp'.format(name), heater.get_float('MinSafeTemp')),
                    gen.add_float_config('{}HeaterMaxSafeTemp'.format(name), heater.get_float('MaxSafeTemp')),
                    gen.add_float_config('{}HeaterControlInterval'.format(name), control_interval),
                    control_service,
                    observer_service,
                    
                ])
            
            gen.add_subst('Heaters', config.do_list('heaters', heater_cb), indent=1)
    
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
