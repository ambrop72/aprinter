import configema as ce

def simple_list(elem_title, value_title, **kwargs):
    return ce.Array(table=True, elem=ce.Compound('elem', title=elem_title, attrs=[
        ce.String(key='value', title=value_title)
    ]), **kwargs)

def oc_unit_choice(**kwargs):
    return ce.Reference(ref_array='id_board.platform', ref_array_descend=['clock', 'avail_oc_units'], ref_id_key='value', ref_name_key='value', title='Output compare unit', deref_key='lalal', **kwargs)

def interrupt_timer_choice(**kwargs):
    return ce.Compound('interrupt_timer', ident='id_interrupt_timer_choice', collapsed=True, attrs=[
        oc_unit_choice(key='oc_unit'),
    ], **kwargs)

def pin_choice(**kwargs):
    return ce.String(**kwargs)

def digital_input_choice(**kwargs):
    return ce.Reference(ref_array='id_configuration.board_data', ref_array_descend=['digital_inputs'], ref_id_key='Name', ref_name_key='Name', **kwargs)

def analog_input_choice(**kwargs):
    return ce.Reference(ref_array='id_configuration.board_data', ref_array_descend=['analog_inputs'], ref_id_key='Name', ref_name_key='Name', **kwargs)

def pwm_output_choice(**kwargs):
    return ce.Reference(ref_array='id_configuration.board_data', ref_array_descend=['pwm_outputs'], ref_id_key='Name', ref_name_key='Name', **kwargs)

def i2c_choice(**kwargs):
    return ce.OneOf(disable_collapse=True, choices=[
        ce.Compound('At91SamI2c', attrs=[
            ce.String(key='Device'),
            ce.Integer(key='Ckdiv'),
            ce.Float(key='I2cFreq')
        ])
    ], **kwargs)

def spi_choice(**kwargs):
    return ce.OneOf(disable_collapse=True, choices=[
        ce.Compound('At91SamSpi', attrs=[
            ce.String(key='Device')
        ]),
        ce.Compound('AvrSpi', attrs=[
            ce.Integer(key='SpeedDiv')
        ]),
    ], **kwargs)

def platform_At91Sam3x8e():
    return ce.Compound('At91Sam3x8e', attrs=[
        ce.Compound('At91Sam3xClock', key='clock', title='Clock', collapsed=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TC{}{}'.format(n, l)
                } for n in range(9) for l in ('A', 'B', 'C')
            ])
        ]),
        ce.Compound('At91SamAdc', key='adc', title='ADC', collapsed=True, attrs=[
            ce.Float(key='freq', title='Frequency'),
            ce.Float(key='avg_interval', title='Averaging interval'),
            ce.Float(key='smoothing', title='Smoothing factor'),
            ce.Integer(key='startup', title='Startup time'),
            ce.Integer(key='settling', title='Settling time'),
            ce.Integer(key='tracking', title='Tracking time'),
            ce.Integer(key='transfer', title='Transfer time')
        ]),
        ce.Compound('At91SamWatchdog', key='watchdog', title='Watchdog', collapsed=True, attrs=[
            ce.Integer(key='Wdv', title='Wdv')
        ]),
        ce.Compound('At91SamPins', key='pins', title='Pins', collapsed=True, attrs=[
            ce.Constant(key='input_modes', value=[
                { 'ident': 'At91SamPinInputModeNormal', 'name': 'Normal' },
                { 'ident': 'At91SamPinInputModePullUp', 'name': 'Pull-up' }
            ])
        ]),
        ce.OneOf(key='pwm', title='PWM module', disable_collapse=True, choices=[
            ce.Compound('Disabled', title='Disabled', disable_collapse=True, attrs=[]),
            ce.Compound('At91Sam3xPwm', title='Enabled', disable_collapse=True, attrs=[
                ce.Integer(key='PreA', title='Prescaler A'),
                ce.Integer(key='DivA', title='Divisor A'),
                ce.Integer(key='PreB', title='Prescaler B'),
                ce.Integer(key='DivB', title='Divisor B'),
            ]),
        ]),
    ])

def platform_Teensy3():
    return ce.Compound('Teensy3', attrs=[
        ce.Compound('Mk20Clock', key='clock', title='Clock', collapsed=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'FTM{}_{}'.format(i, j)
                } for i in range(2) for j in range({0: 8, 1: 2}[i])
            ])
        ]),
        ce.Compound('Mk20Adc', key='adc', title='ADC', collapsed=True, attrs=[
            ce.Integer(key='AdcADiv', title='AdcADiv'),
        ]),
        ce.Compound('Mk20Watchdog', key='watchdog', title='Watchdog', collapsed=True, attrs=[
            ce.Integer(key='Toval', title='Timeout value'),
            ce.Integer(key='Prescval', title='Prescaler value'),
        ]),
        ce.Compound('Mk20Pins', key='pins', title='Pins', collapsed=True, attrs=[
            ce.Constant(key='input_modes', value=[
                { 'ident': 'Mk20PinInputModeNormal', 'name': 'Normal' },
                { 'ident': 'Mk20PinInputModePullUp', 'name': 'Pull-up' },
                { 'ident': 'Mk20PinInputModePullDown', 'name': 'Pull-down' },
            ])
        ]),
    ])

def platform_Avr(variant):
    if variant == 'ATmega2560':
        timers = (range(6), lambda i: ('A', 'B') + (() if i in (0, 2) else ('C',)))
    elif variant == 'ATmega1284p':
        timers = (range(4), lambda i: ('A', 'B'))
    else:
        assert False
    
    return ce.Compound('AVR {}'.format(variant), attrs=[
        ce.Compound('AvrClock', key='clock', title='Clock', collapsed=True, attrs=[
            ce.Integer(key='PrescaleDivide', title='Prescaler (as division factor)'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TC{}_{}'.format(i, j)
                } for i in timers[0] for j in timers[1](i)
            ]),
            ce.Array(key='timers', title='Timer configuration', disable_collapse=True, elem=ce.Compound('Timer', title_key='Timer', collapsed=True, attrs=[
                ce.String(key='Timer'),
                ce.OneOf(key='Mode', title='Mode', disable_collapse=True, choices=[
                    ce.Compound('AvrClockTcModeClock', title='Normal (for iterrupt-timers)', disable_collapse=True, attrs=[]),
                    ce.Compound('AvrClockTcMode8BitPwm', title='PWM 8-bit (for Hard-PWM)', disable_collapse=True, attrs=[
                        ce.Integer(key='PrescaleDivide'),
                    ]),
                    ce.Compound('AvrClockTcMode16BitPwm', title='PWM 16-bit (for Hard-PWM)', disable_collapse=True, attrs=[
                        ce.Integer(key='PrescaleDivide'),
                        ce.Integer(key='TopVal'),
                    ]),
                ]),
            ]))
        ]),
        ce.Compound('AvrAdc', key='adc', title='ADC', collapsed=True, attrs=[
            ce.Integer(key='RefSel'),
            ce.Integer(key='Prescaler'),
        ]),
        ce.Compound('AvrWatchdog', key='watchdog', title='Watchdog', collapsed=True, attrs=[
            ce.String(key='Timeout', title='Timeout (WDTO_*)'),
        ]),
        ce.Compound('AvrPins', key='pins', title='Pins', collapsed=True, attrs=[
            ce.Constant(key='input_modes', value=[
                { 'ident': 'AvrPinInputModeNormal', 'name': 'Normal' },
                { 'ident': 'AvrPinInputModePullUp', 'name': 'Pull-up' },
            ])
        ]),
    ])

def hard_pwm_choice(**kwargs):
    return ce.OneOf(title='Hard-PWM driver', disable_collapse=True, choices=[
        ce.Compound('AvrClockPwm', ident='id_pwm_output', attrs=[
            oc_unit_choice(key='oc_unit'),
            pin_choice(key='OutputPin', title='Output pin (determined by OC unit)'),
        ]),
        ce.Compound('At91Sam3xPwmChannel', attrs=[
            ce.Integer(key='ChannelPrescaler', title='Channel prescaler'),
            ce.Integer(key='ChannelPeriod', title='Channel period value'),
            ce.Integer(key='ChannelNumber', title='Channel number'),
            pin_choice(key='OutputPin', title='Output pin (constrained by choice of channel/signal)'),
            ce.String(key='Signal', title='Connection type (L/H)'),
        ]),
    ], **kwargs)

def editor():
    return ce.Compound('editor', title='Configuration editor', disable_collapse=True, no_header=True, ident='id_editor', attrs=[
        ce.Constant(key='version', value=1),
        ce.Reference(key='selected_config', title='Selected configuration (to compile)', ref_array='configurations', ref_id_key='name', ref_name_key='name'),
        ce.Array(key='configurations', title='Configurations', processing_order=-1, elem=ce.Compound('config', key='config', ident='id_configuration', title='Configuration', title_key='name', collapsed=True, attrs=[
            ce.String(key='name', title='Configuration name', default='New Configuration'),
            ce.Reference(key='board_id', ref_array='boards', ref_id_key='identifier', ref_name_key='name', deref_key='board_data', title='Board', processing_order=-1),
            ce.Float(key='InactiveTime', title='Disable steppers after [s]', default=480),
            ce.Compound('advanced', key='advanced', title='Advanced parameters', collapsed=True, attrs=[
                ce.Float(key='LedBlinkInterval', title='LED blink interval [s]', default=0.5),
                ce.Float(key='ForceTimeout', title='Force motion timeout [s]', default=0.1),
            ]),
            ce.Array(key='steppers', title='Steppers', disable_collapse=True, elem=ce.Compound('stepper', title='Stepper', title_key='Name', collapsed=True, ident='id_configuration_stepper', attrs=[
                ce.String(key='Name', title='Name (cartesian X/Y/Z, extruders E/U/V, delta A/B/C)'),
                ce.Reference(key='stepper_port', title='Stepper port', ref_array='id_configuration.board_data', ref_array_descend=['stepper_ports'], ref_id_key='Name', ref_name_key='Name'),
                ce.Boolean(key='InvertDir', title='Invert direction', false_title='No (high StepPin is positive motion)', true_title='Yes (high StepPin is negative motion)', default=False),
                ce.Float(key='StepsPerUnit', title='Steps per unit [1/mm]', default=80),
                ce.Float(key='MinPos', title='Minimum position [mm] (~-40000 for extruders)', default=0),
                ce.Float(key='MaxPos', title='Maximum position [mm] (~40000 for extruders)', default=200),
                ce.Float(key='MaxSpeed', title='Maximum speed [mm/s]', default=300),
                ce.Float(key='MaxAccel', title='Maximum acceleration [mm/s^2]', default=1500),
                ce.Float(key='DistanceFactor', title='Distance factor [1]', default=1),
                ce.Float(key='CorneringDistance', title='Cornering distance [step]', default=40),
                ce.Boolean(key='EnableCartesianSpeedLimit', title='Is cartesian (Yes for X/Y/Z, No for extruders)', default=True),
                ce.OneOf(key='homing', title='Homing', collapsed=True, choices=[
                    ce.Compound('no_homing', title='Disabled', attrs=[]),
                    ce.Compound('homing', title='Enabled', ident='id_board_steppers_homing', attrs=[
                        ce.Boolean(key='HomeDir', title='Homing direction', false_title='Negative', true_title='Positive', default=False),
                        digital_input_choice(key='HomeEndstopInput', title='Endstop digital input'),
                        ce.Boolean(key='HomeEndInvert', title='Invert endstop', false_title='No (high signal is pressed)', true_title='Yes (low signal is pressed)', default=False),
                        ce.Float(key='HomeFastMaxDist', title='Maximum fast travel [mm] (use more than abs(MinPos-MaxPos))', default=250),
                        ce.Float(key='HomeRetractDist', title='Retraction travel [mm]', default=3),
                        ce.Float(key='HomeSlowMaxDist', title='Maximum slow travel [mm] (use more than RetractionTravel)', default=5),
                        ce.Float(key='HomeFastSpeed', title='Fast speed [mm/s]', default=40),
                        ce.Float(key='HomeRetractSpeed', title='Retraction speed [mm/s]', default=50),
                        ce.Float(key='HomeSlowSpeed', title='Slow speed [mm/s]', default=5)
                    ])
                ])
            ])),
            ce.Array(key='heaters', title='Heaters', disable_collapse=True, elem=ce.Compound('heater', title='Heater', title_key='Name', collapsed=True, ident='id_configuration_heater', attrs=[
                ce.String(key='Name', title='Name (single character, T=extruder, B=bed)'),
                pwm_output_choice(key='pwm_output', title='PWM output'),
                ce.Integer(key='SetMCommand', title='Set command M-number (extruder 104, bed 140)', default=104),
                ce.Integer(key='WaitMCommand', title='Wait command M-number (extruder 109, bed 190)', default=109),
                analog_input_choice(key='ThermistorInput', title='Thermistor analog input'),
                ce.Float(key='MinSafeTemp', title='Turn off if temperature is below [C]', default=15),
                ce.Float(key='MaxSafeTemp', title='Turn off if temperature is above [C]', default=280),
                ce.Compound('conversion', key='conversion', title='Conversion parameters', collapsed=True, attrs=[
                    ce.Float(key='ResistorR', title='Series-resistor resistance [ohm]', default=4700),
                    ce.Float(key='R0', title='Thermistor resistance @25C [ohm]', default=100000),
                    ce.Float(key='Beta', title='Thermistor beta value [K]', default=3960),
                    ce.Float(key='MinTemp', title='Reliable measurements are above [C]', default=10),
                    ce.Float(key='MaxTemp', title='Reliable measurements are below [C]', default=300)
                ]),
                ce.Compound('control', key='control', title='PID control parameters', collapsed=True, attrs=[
                    ce.Float(key='ControlInterval', title='Invoke the PID control algorithm every [s]', default=0.2),
                    ce.Float(key='PidP', title='Proportional factor [1/K]', default=0.05),
                    ce.Float(key='PidI', title='Integral factor [1/(Ks)]', default=0.0005),
                    ce.Float(key='PidD', title='Derivative factor [s/K]', default=0.2),
                    ce.Float(key='PidIStateMin', title='Lower bound of the integral value [1]', default=0.0),
                    ce.Float(key='PidIStateMax', title='Upper bound of the integral value [1]', default=0.6),
                    ce.Float(key='PidDHistory', title='Smoothing factor for derivative estimation [1]', default=0.7)
                ]),
                ce.Compound('observer', key='observer', title='Temperature-reached semantics', collapsed=True, attrs=[
                    ce.Float(key='ObserverTolerance', title='The temperature must be within [K]', default=3),
                    ce.Float(key='ObserverMinTime', title='For at least this long [s]', default=3),
                    ce.Float(key='ObserverInterval', title='With a measurement taken each [s]', default=0.5),
                ])
            ])),
            ce.Array(key='fans', title='Fans', disable_collapse=True, elem=ce.Compound('fan', title='Fan', title_key='Name', collapsed=True, ident='id_configuration_fan', attrs=[
                ce.String(key='Name (single character, e.g. the same as corresponding extruder)', title='Name'),
                pwm_output_choice(key='pwm_output', title='PWM output'),
                ce.Integer(key='SetMCommand', title='Set-command M-number (106 for first fan)'),
                ce.Integer(key='OffMCommand', title='Off-command M-number (107 for first fan)'),
            ])),
            ce.OneOf(key='probe', title='Bed probing', collapsed=True, choices=[
                ce.Compound('NoProbe', title='Disabled', attrs=[]),
                ce.Compound('Probe', title='Enabled', ident='id_configuration_probe_probe', attrs=[
                    digital_input_choice(key='ProbePin', title='Probe switch pin'),
                    ce.Boolean(key='InvertInput', title='Invert switch input', false_title='No (high signal is pressed)', true_title='Yes (low signal is pressed)'),
                    ce.Float(key='OffsetX', title='X-offset of probe from logical position [mm]', default=0),
                    ce.Float(key='OffsetY', title='Y-offset of probe from logical position [mm]', default=0),
                    ce.Float(key='StartHeight', title='Starting Z for probing a point [mm]', default=10),
                    ce.Float(key='LowHeight', title='Minimum Z to move down to [mm]', default=2),
                    ce.Float(key='RetractDist', title='Retraction distance [mm]', default=1),
                    ce.Float(key='MoveSpeed', title='Speed for moving to probe points [mm/s]', default=200),
                    ce.Float(key='FastSpeed', title='Fast probing speed [mm/s]', default=2),
                    ce.Float(key='RetractSpeed', title='Retraction speed [mm/s]', default=10),
                    ce.Float(key='SlowSpeed', title='Slow probing speed [mm/s]', default=0.5),
                    ce.Array(key='ProbePoints', title='Coordinates of probing points', disable_collapse=True, table=True, elem=ce.Compound('ProbePoint', title='Probe point', attrs=[
                        ce.Float(key='X'),
                        ce.Float(key='Y')
                    ]))
                ])
            ])
        ])),
        ce.Array(key='boards', title='Boards', processing_order=-2, elem=ce.Compound('board', title='Board', title_key='name', collapsed=True, ident='id_board', attrs=[
            ce.String(key='identifier', title='Identifier'),
            ce.String(key='name', title='Name'),
            ce.String(key='board_for_build', title='Board for building (see nix/boards.nix)'),
            ce.String(key='output_type', title='Build output type', enum=['hex', 'bin']),
            pin_choice(key='LedPin', title='LED pin'),
            ce.OneOf(key='config_manager', title='Runtime configuration', collapsed=True, choices=[
                ce.Compound('ConstantConfigManager', title='Disabled', attrs=[]),
                ce.Compound('RuntimeConfigManager', title='Enabled', attrs=[
                    ce.OneOf(key='ConfigStore', title='Configuration storage', disable_collapse=True, choices=[
                        ce.Compound('NoStore', title='None', attrs=[]),
                        ce.Compound('EepromConfigStore', attrs=[
                            ce.OneOf(key='Eeprom', title='EEPROM backend', disable_collapse=True, choices=[
                                ce.Compound('I2cEeprom', attrs=[
                                    i2c_choice(key='I2c', title='I2C backend'),
                                    ce.Integer(key='I2cAddr'),
                                    ce.Integer(key='Size'),
                                    ce.Integer(key='BlockSize'),
                                    ce.Float(key='WriteTimeout')
                                ]),
                                ce.Compound('TeensyEeprom', attrs=[
                                    ce.Integer(key='Size'),
                                    ce.Integer(key='FakeBlockSize'),
                                ]),
                                ce.Compound('AvrEeprom', attrs=[
                                    ce.Integer(key='FakeBlockSize'),
                                ]),
                            ]),
                            ce.Integer(key='StartBlock'),
                            ce.Integer(key='EndBlock'),
                        ])
                    ])
                ]),
            ]),
            ce.Compound('serial', key='serial', title='Serial parameters', collapsed=True, attrs=[
                ce.Integer(key='BaudRate', title='Baud rate'),
                ce.Integer(key='RecvBufferSizeExp', title='Receive buffer size (power of two exponent)'),
                ce.Integer(key='SendBufferSizeExp', title='Send buffer size (power of two exponent)'),
                ce.Integer(key='GcodeMaxParts', title='Max parts in GCode command'),
                ce.OneOf(key='Service', title='Backend', disable_collapse=True, choices=[
                    ce.Compound('AsfUsbSerial', title='AT91 USB', attrs=[]),
                    ce.Compound('At91Sam3xSerial', title='AT91 UART', attrs=[]),
                    ce.Compound('TeensyUsbSerial', title='Teensy3 USB', attrs=[]),
                    ce.Compound('AvrSerial', title='AVR UART', attrs=[
                        ce.Boolean(key='DoubleSpeed'),
                    ]),
                ])
            ]),
            ce.OneOf(key='sdcard', title='SD card', collapsed=True, choices=[
                ce.Compound('NoSdCard', title='Disabled', attrs=[]),
                ce.Compound('SdCard', title='Enabled', attrs=[
                    ce.Integer(key='BufferBaseSize', title='Buffer size'),
                    ce.Integer(key='MaxCommandSize', title='Maximum command size'),
                    ce.OneOf(key='GcodeParser', title='G-code parser', choices=[
                        ce.Compound('TextGcodeParser', title='Text G-code parser', attrs=[
                            ce.Integer(key='MaxParts', title='Maximum number of command parts')
                        ]),
                        ce.Compound('BinaryGcodeParser', title='Binary G-code parser', attrs=[
                            ce.Integer(key='MaxParts', title='Maximum number of command parts')
                        ])
                    ]),
                    ce.OneOf(key='SdCardService', title='Driver', collapsed=True, choices=[
                        ce.Compound('SpiSdCard', title='SPI', attrs=[
                            pin_choice(key='SsPin', title='SS pin'),
                            spi_choice(key='SpiService', title='SPI driver')
                        ])
                    ])
                ])
            ]),
            ce.Compound('performance', key='performance', title='Performance parameters', collapsed=True, attrs=[
                ce.Float(key='MaxStepsPerCycle', title='Max steps per cycle'),
                ce.Integer(key='StepperSegmentBufferSize', title='Stepper segment buffer size'),
                ce.Integer(key='EventChannelBufferSize', title='Event channel buffer size'),
                ce.Integer(key='LookaheadBufferSize', title='Lookahead buffer size'),
                ce.Integer(key='LookaheadCommitCount', title='Lookahead commit count'),
                ce.String(key='FpType', enum=['float', 'double']),
                ce.String(key='AxisDriverPrecisionParams', title='Stepping precision parameters', enum=['AxisDriverAvrPrecisionParams', 'AxisDriverDuePrecisionParams']),
                ce.Float(key='EventChannelTimerClearance', title='Event channel timer clearance')
            ]),
            interrupt_timer_choice(key='EventChannelTimer', title='Event channel timer'),
            ce.Array(key='stepper_ports', title='Stepper ports', disable_collapse=True, elem=ce.Compound('stepper_port', title='Stepper port', title_key='Name', collapsed=True, attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='DirPin', title='Direction pin'),
                pin_choice(key='StepPin', title='Step pin'),
                pin_choice(key='EnablePin', title='Enable pin'),
                interrupt_timer_choice(key='StepperTimer', title='Stepper timer'),
            ])),
            ce.Array(key='digital_inputs', title='Digital inputs', disable_collapse=True, elem=ce.Compound('digital_input', title='Digital input', title_key='Name', collapsed=True, ident='id_board_digital_inputs', attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='Pin', title='Pin'),
                ce.Reference(key='InputMode', title='Input mode', ref_array='id_board.platform', ref_array_descend=['pins', 'input_modes'], ref_id_key='ident', ref_name_key='name')
            ])),
            ce.Array(key='analog_inputs', title='Analog inputs', disable_collapse=True, elem=ce.Compound('analog_input', title='Analog input', title_key='Name', collapsed=True, attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='Pin', title='Pin'),
            ])),
            ce.Array(key='pwm_outputs', title='PWM outputs', disable_collapse=True, elem=ce.Compound('pwm_output', title='PWM output', title_key='Name', collapsed=True, attrs=[
                ce.String(key='Name', title='Name'),
                ce.OneOf(key='Backend', title='Backend', disable_collapse=True, choices=[
                    ce.Compound('SoftPwm', attrs=[
                        pin_choice(key='OutputPin', title='Output pin'),
                        ce.Boolean(key='OutputInvert', title='Output logic', false_title='Normal (On=High)', true_title='Inverted (On=Low)'),
                        ce.Float(key='PulseInterval', title='PWM pulse duration'),
                        interrupt_timer_choice(key='Timer', title='Soft PWM Timer')
                    ]),
                    ce.Compound('HardPwm', attrs=[
                        hard_pwm_choice(key='HardPwmDriver'),
                    ]),
                ])
            ])),
            ce.OneOf(key='platform', title='Platform', disable_collapse=True, processing_order=-1, choices=[
                platform_At91Sam3x8e(),
                platform_Teensy3(),
                platform_Avr('ATmega2560'),
                platform_Avr('ATmega1284p'),
            ]),
            ce.Array(key='board_helper_includes', title='Board helper includes', disable_collapse=True, table=True, elem=ce.String(title='Name')),
        ]))
    ])
