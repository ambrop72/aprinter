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

import configema as ce

def oc_unit_choice(**kwargs):
    return ce.Reference(ref_array={'base': 'id_board.platform_config.platform', 'descend': ['clock', 'avail_oc_units']}, ref_id_key='value', ref_name_key='value', title='Output compare unit', **kwargs)

def interrupt_timer_choice(**kwargs):
    return ce.Compound('interrupt_timer', ident='id_interrupt_timer_choice', attrs=[
        oc_unit_choice(key='oc_unit'),
    ], **kwargs)

def pin_choice(**kwargs):
    return ce.String(**kwargs)

def digital_input_choice(**kwargs):
    return ce.Reference(ref_array={'base': 'id_configuration.board_data', 'descend': ['digital_inputs']}, ref_id_key='Name', ref_name_key='Name', **kwargs)

def analog_input_choice(**kwargs):
    return ce.Reference(ref_array={'base': 'id_configuration.board_data', 'descend': ['analog_inputs']}, ref_id_key='Name', ref_name_key='Name', **kwargs)

def pwm_output_choice(context, **kwargs):
    return ce.Reference(ref_array=context.board_ref(['pwm_outputs']), ref_id_key='Name', ref_name_key='Name', **kwargs)

def i2c_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('At91SamI2c', attrs=[
            ce.String(key='Device'),
            ce.Integer(key='Ckdiv'),
            ce.Float(key='I2cFreq')
        ])
    ], **kwargs)

def spi_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('At91SamSpi', attrs=[
            ce.String(key='Device')
        ]),
        ce.Compound('AvrSpi', attrs=[
            ce.Integer(key='SpeedDiv')
        ]),
    ], **kwargs)

def sdio_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('Stm32f4Sdio', attrs=[
            ce.Boolean(key='IsWideMode', title='Data bus width', false_title='1-bit', true_title='4-bit', default=False),
            ce.Integer(key='DataTimeoutBusClocks', title='Data timeout (in SDIO bus clocks)', default=0x20000000),
        ]),
    ], **kwargs)

def watchdog_at91sam():
    return ce.Compound('At91SamWatchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
        ce.Integer(key='Wdv', title='Wdv')
    ])

def pins_at91sam():
    return ce.Compound('At91SamPins', key='pins', title='Pins', collapsable=True, attrs=[
        ce.Constant(key='input_modes', value=[
            { 'ident': 'At91SamPinInputModeNormal', 'name': 'Normal' },
            { 'ident': 'At91SamPinInputModePullUp', 'name': 'Pull-up' }
        ])
    ])

def platform_At91Sam3x8e():
    return ce.Compound('At91Sam3x8e', attrs=[
        ce.Compound('At91Sam3xClock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TC{}{}'.format(n, l)
                } for n in range(9) for l in ('A', 'B', 'C')
            ])
        ]),
        ce.Compound('At91SamAdc', key='adc', title='ADC', collapsable=True, attrs=[
            ce.Float(key='freq', title='Frequency'),
            ce.Float(key='avg_interval', title='Averaging interval'),
            ce.Float(key='smoothing', title='Smoothing factor'),
            ce.Integer(key='startup', title='Startup time'),
            ce.Integer(key='settling', title='Settling time'),
            ce.Integer(key='tracking', title='Tracking time'),
            ce.Integer(key='transfer', title='Transfer time')
        ]),
        watchdog_at91sam(),
        pins_at91sam(),
        ce.OneOf(key='pwm', title='PWM module', choices=[
            ce.Compound('Disabled', title='Disabled', attrs=[]),
            ce.Compound('At91Sam3xPwm', title='Enabled', attrs=[
                ce.Integer(key='PreA', title='Prescaler A'),
                ce.Integer(key='DivA', title='Divisor A'),
                ce.Integer(key='PreB', title='Prescaler B'),
                ce.Integer(key='DivB', title='Divisor B'),
            ]),
        ]),
    ])

def platform_At91Sam3u4e():
    return ce.Compound('At91Sam3u4e', attrs=[
        ce.Compound('At91Sam3uClock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TC{}{}'.format(n, l)
                } for n in range(3) for l in ('A', 'B', 'C')
            ])
        ]),
        ce.Compound('At91Sam3uAdc', key='adc', title='ADC', collapsable=True, attrs=[
            ce.Float(key='freq', title='Frequency'),
            ce.Float(key='avg_interval', title='Averaging interval'),
            ce.Float(key='smoothing', title='Smoothing factor'),
            ce.Integer(key='startup', title='Startup time'),
            ce.Integer(key='shtim', title='Sample and Hold time'),
        ]),
        watchdog_at91sam(),
        pins_at91sam(),
    ])

def platform_Teensy3():
    return ce.Compound('Teensy3', attrs=[
        ce.Compound('Mk20Clock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'FTM{}_{}'.format(i, j)
                } for i in range(2) for j in range({0: 8, 1: 2}[i])
            ])
        ]),
        ce.Compound('Mk20Adc', key='adc', title='ADC', collapsable=True, attrs=[
            ce.Integer(key='AdcADiv', title='AdcADiv'),
        ]),
        ce.Compound('Mk20Watchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
            ce.Integer(key='Toval', title='Timeout value'),
            ce.Integer(key='Prescval', title='Prescaler value'),
        ]),
        ce.Compound('Mk20Pins', key='pins', title='Pins', collapsable=True, attrs=[
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
        ce.Compound('AvrClock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='PrescaleDivide', title='Prescaler (as division factor)'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TC{}_{}'.format(i, j)
                } for i in timers[0] for j in timers[1](i)
            ]),
            ce.Array(key='timers', title='Timer configuration', elem=ce.Compound('Timer', title_key='Timer', collapsable=True, attrs=[
                ce.String(key='Timer'),
                ce.OneOf(key='Mode', title='Mode', choices=[
                    ce.Compound('AvrClockTcModeClock', title='Normal (for interrupt-timers)', attrs=[]),
                    ce.Compound('AvrClockTcMode8BitPwm', title='PWM 8-bit (for Hard-PWM)', attrs=[
                        ce.Integer(key='PrescaleDivide'),
                    ]),
                    ce.Compound('AvrClockTcMode16BitPwm', title='PWM 16-bit (for Hard-PWM)', attrs=[
                        ce.Integer(key='PrescaleDivide'),
                        ce.Integer(key='TopVal'),
                    ]),
                ]),
            ]))
        ]),
        ce.Compound('AvrAdc', key='adc', title='ADC', collapsable=True, attrs=[
            ce.Integer(key='RefSel'),
            ce.Integer(key='Prescaler'),
        ]),
        ce.Compound('AvrWatchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
            ce.String(key='Timeout', title='Timeout (WDTO_*)'),
        ]),
        ce.Compound('AvrPins', key='pins', title='Pins', collapsable=True, attrs=[
            ce.Constant(key='input_modes', value=[
                { 'ident': 'AvrPinInputModeNormal', 'name': 'Normal' },
                { 'ident': 'AvrPinInputModePullUp', 'name': 'Pull-up' },
            ])
        ]),
    ])

def platform_Stm32f4():
    return ce.Compound('Stm32f4', attrs=[
        ce.Compound('Stm32f4Clock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TIM{}_{}'.format(n, m)
                } for n in [2,3,4,5] for m in [1,2,3,4]
            ])
        ]),
        ce.Compound('Stm32f4Adc', key='adc', title='ADC', collapsable=True, attrs=[
            ce.Integer(key='ClockDivider'),
            ce.Integer(key='SampleTimeSelection'),
        ]),
        ce.Compound('Stm32f4Watchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
            ce.Integer(key='Divider', title='Divider'),
            ce.Integer(key='Reload', title='Reload'),
        ]),
        ce.Compound('Stm32f4Pins', key='pins', title='Pins', collapsable=True, attrs=[
            ce.Constant(key='input_modes', value=[
                { 'ident': 'Stm32f4PinInputModeNormal', 'name': 'Normal' },
                { 'ident': 'Stm32f4PinInputModePullUp', 'name': 'Pull-up' },
                { 'ident': 'Stm32f4PinInputModePullDown', 'name': 'Pull-down' },
            ])
        ]),
    ])

def hard_pwm_choice(**kwargs):
    return ce.OneOf(title='Hard-PWM driver', choices=[
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

def homing_params(**kwargs):
    return ce.OneOf(title='Homing', choices=[
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
    ], **kwargs)

def make_transform_type(transform_type, transform_title, segments_per_sec_relevant, stepper_defs, axis_defs, specific_params):
    assert len(stepper_defs) == len(axis_defs)
    
    return ce.Compound(transform_type, title=transform_title, attrs=(
        specific_params +
        (
            [ce.Float(key='SegmentsPerSecond', title='Max segments per second', default=100)] if segments_per_sec_relevant else
            [ce.Constant(key='SegmentsPerSecond', value=0)]
        ) +
        [
            ce.Constant(key='DimensionCount', value=len(stepper_defs)),
            ce.Compound('Steppers', key='Steppers', title='Stepper mapping', attrs=[
                ce.Compound('TransformStepperParams', key='TransformStepper{}'.format(i), title=stepper_def['title'], collapsable=True, attrs=[
                    ce.String(key='StepperName', title='Name of stepper to use', default=stepper_def['default_name']),
                ])
                for (i, stepper_def) in enumerate(stepper_defs)
            ]),
            ce.Compound('CartesianAxes', key='CartesianAxes', title='Cartesian axes', attrs=[
                ce.Compound('VirtualAxisParams', key='VirtualAxis{}'.format(i), title='Cartesian axis {}'.format(axis_def['axis_name']), collapsable=True, attrs=(
                    [
                        ce.Constant(key='Name', value=axis_def['axis_name']),
                        ce.Float(key='MinPos', title='Minimum position [mm]', default=0),
                        ce.Float(key='MaxPos', title='Maximum position [mm]', default=200),
                        ce.Float(key='MaxSpeed', title='Maximum speed [mm/s]', default=300),
                    ] +
                    (
                        [homing_params(key='homing')] if axis_def['homing_allowed'] else
                        [ce.Constant(key='homing', value={'_compoundName': 'no_homing'})]
                    )
                ))
                for (i, axis_def) in enumerate(axis_defs)
            ]),
        ]
    ))

def stepper_port_reference(context):
    return ce.Reference(key='stepper_port', title='Stepper port', ref_array=context.board_ref(['stepper_ports']), ref_id_key='Name', ref_name_key='Name')

def microstep_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('A4982', attrs=[
            pin_choice(key='Ms1Pin', title='MS1 output pin'),
            pin_choice(key='Ms2Pin', title='MS2 output pin'),
        ]),
        ce.Compound('A4988', attrs=[
            pin_choice(key='Ms1Pin', title='MS1 output pin'),
            pin_choice(key='Ms2Pin', title='MS2 output pin'),
            pin_choice(key='Ms3Pin', title='MS3 output pin'),
        ]),
    ], **kwargs)

def current_driver_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('Ad5206Current', title='AD5206', attrs=[
            pin_choice(key='SsPin', title='SS pin'),
            spi_choice(key='SpiService', title='SPI driver'),
        ]),
    ], **kwargs)

def current_driver_channel_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('Ad5206CurrentChannelParams', title='AD5206 channel', attrs=[
            ce.Integer(key='DevChannelIndex', title='Device channel index'),
            ce.Float(key='ConversionFactor', title='Current conversion factor'),
        ]),
    ], **kwargs)

def flash_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('At91SamFlash', title='AT91 flash', attrs=[
            ce.Integer(key='DeviceIndex', title='Flash device index'),
        ]),
    ], **kwargs)

class ConfigurationContext(object):
    def board_ref(self, what):
        return {'base': 'id_configuration.board_data', 'descend': what}

class BoardContext(object):
    def board_ref(self, what):
        return {'base': 'id_board.{}'.format(what[0]), 'descend': what[1:]}

configuration_context = ConfigurationContext()
board_context = BoardContext()

def editor():
    return ce.Compound('editor', title='Configuration editor', no_header=True, ident='id_editor', attrs=[
        ce.Constant(key='version', value=1),
        ce.Reference(key='selected_config', title='Selected configuration (to compile)', ref_array={'base': 'id_editor.configurations', 'descend': []}, ref_id_key='name', ref_name_key='name'),
        ce.Array(key='configurations', title='Configurations', processing_order=-1, copy_name_key='name', elem=ce.Compound('config', key='config', ident='id_configuration', title='Configuration', title_key='name', collapsable=True, attrs=[
            ce.String(key='name', title='Configuration name', default='New Configuration'),
            ce.Reference(key='board', ref_array={'base': 'id_editor.boards', 'descend': []}, ref_id_key='name', ref_name_key='name', deref_key='board_data', title='Board', processing_order=-1),
            ce.Float(key='InactiveTime', title='Disable steppers after [s]', default=480),
            ce.Float(key='WaitTimeout', title='Timeout when waiting for heater temperatures (M116) [s]', default=500),
            ce.Compound('advanced', key='advanced', title='Advanced parameters', collapsable=True, attrs=[
                ce.Float(key='LedBlinkInterval', title='LED blink interval [s]', default=0.5),
                ce.Float(key='ForceTimeout', title='Force motion timeout [s]', default=0.1),
            ]),
            ce.Array(key='steppers', title='Steppers', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('stepper', title='Stepper', title_key='Name', collapsable=True, ident='id_configuration_stepper', attrs=[
                ce.String(key='Name', title='Name (cartesian X/Y/Z, extruders E/U/V, delta A/B/C)'),
                stepper_port_reference(configuration_context),
                ce.Boolean(key='InvertDir', title='Invert direction', false_title='No (high StepPin is positive motion)', true_title='Yes (high StepPin is negative motion)', default=False),
                ce.Array(key='slave_steppers', title='Slave steppers', table=True, elem=ce.Compound('slave_stepper', ident='id_slave_stepper', title='Slave stepper', attrs=[
                    stepper_port_reference(configuration_context),
                    ce.Boolean(key='InvertDir', title='Invert direction', default=False),
                ])),
                ce.Integer(key='MicroSteps', title='Micro-steps (if board supports micro-step configuration)', default=0),
                ce.Float(key='Current', title='Motor current (if board supports current control) [mA]', default=0),
                ce.Float(key='StepsPerUnit', title='Steps per unit [1/mm]', default=80),
                ce.Float(key='MinPos', title='Minimum position [mm] (~-40000 for extruders)', default=0),
                ce.Float(key='MaxPos', title='Maximum position [mm] (~40000 for extruders)', default=200),
                ce.Float(key='MaxSpeed', title='Maximum speed [mm/s]', default=300),
                ce.Float(key='MaxAccel', title='Maximum acceleration [mm/s^2]', default=1500),
                ce.Float(key='DistanceFactor', title='Distance factor [1]', default=1),
                ce.Float(key='CorneringDistance', title='Cornering distance [step]', default=40),
                ce.Boolean(key='EnableCartesianSpeedLimit', title='Is cartesian (Yes for X/Y/Z, No for extruders)', default=True),
                homing_params(key='homing'),
            ])),
            ce.OneOf(key='transform', title='Coordinate transformation', choices=[
                ce.Compound('NoTransform', title='None (cartesian)', attrs=[]),
                make_transform_type(transform_type='CoreXY', transform_title='CoreXY/H-bot', segments_per_sec_relevant=False,
                    stepper_defs=[
                        {'default_name': 'A', 'title': 'First stepper'},
                        {'default_name': 'B', 'title': 'Second stepper'},
                    ],
                    axis_defs=[
                        {'axis_name': 'X', 'homing_allowed': True},
                        {'axis_name': 'Y', 'homing_allowed': True},
                    ],
                    specific_params=[]
                ),
                make_transform_type(transform_type='Delta', transform_title='Delta', segments_per_sec_relevant=True,
                    stepper_defs=[
                        {'default_name': 'A', 'title': 'Tower-1 stepper (bottom-left)'},
                        {'default_name': 'B', 'title': 'Tower-2 stepper (bottom-right)'},
                        {'default_name': 'C', 'title': 'Tower-3 stepper (top)'},
                    ],
                    axis_defs=[
                        {'axis_name': 'X', 'homing_allowed': False},
                        {'axis_name': 'Y', 'homing_allowed': False},
                        {'axis_name': 'Z', 'homing_allowed': False},
                    ],
                    specific_params=[
                        ce.Float(key='DiagnalRod', title='Diagonal rod length [mm]', default=214),
                        ce.Float(key='SmoothRodOffset', title='Smooth rod offset [mm]', default=145),
                        ce.Float(key='EffectorOffset', title='Effector offset [mm]', default=19.9),
                        ce.Float(key='CarriageOffset', title='Carriage offset [mm]', default=19.5),
                        ce.Float(key='MinSplitLength', title='Minimum segment length for splitting [mm]', default=0.1),
                        ce.Float(key='MaxSplitLength', title='Maximum segment length for splitting [mm]', default=4.0),
                    ]
                ),
            ]),
            ce.Array(key='heaters', title='Heaters', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('heater', title='Heater', title_key='Name', collapsable=True, ident='id_configuration_heater', attrs=[
                ce.String(key='Name', title='Name (single character, T=extruder, B=bed)'),
                pwm_output_choice(configuration_context, key='pwm_output', title='PWM output'),
                ce.Integer(key='SetMCommand', title='Set command M-number (extruder 104, bed 140)', default=104),
                analog_input_choice(key='ThermistorInput', title='Thermistor analog input'),
                ce.Float(key='MinSafeTemp', title='Turn off if temperature is below [C]', default=10),
                ce.Float(key='MaxSafeTemp', title='Turn off if temperature is above [C]', default=280),
                ce.Compound('conversion', key='conversion', title='Conversion parameters', attrs=[
                    ce.Float(key='ResistorR', title='Series-resistor resistance [ohm]', default=4700),
                    ce.Float(key='R0', title='Thermistor resistance @25C [ohm]', default=100000),
                    ce.Float(key='Beta', title='Thermistor beta value [K]', default=3960),
                    ce.Float(key='MinTemp', title='Reliable measurements are above [C]', default=10),
                    ce.Float(key='MaxTemp', title='Reliable measurements are below [C]', default=300)
                ]),
                ce.Compound('control', key='control', title='PID control parameters', attrs=[
                    ce.Float(key='ControlInterval', title='Invoke the PID control algorithm every [s]', default=0.2),
                    ce.Float(key='PidP', title='Proportional factor [1/K]', default=0.05),
                    ce.Float(key='PidI', title='Integral factor [1/(Ks)]', default=0.0005),
                    ce.Float(key='PidD', title='Derivative factor [s/K]', default=0.2),
                    ce.Float(key='PidIStateMin', title='Lower bound of the integral value [1]', default=0.0),
                    ce.Float(key='PidIStateMax', title='Upper bound of the integral value [1]', default=0.6),
                    ce.Float(key='PidDHistory', title='Smoothing factor for derivative estimation [1]', default=0.7)
                ]),
                ce.Compound('observer', key='observer', title='Temperature-reached semantics', attrs=[
                    ce.Float(key='ObserverTolerance', title='The temperature must be within [K]', default=3),
                    ce.Float(key='ObserverMinTime', title='For at least this long [s]', default=3),
                    ce.Float(key='ObserverInterval', title='With a measurement taken each [s]', default=0.5),
                ])
            ])),
            ce.Array(key='fans', title='Fans', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('fan', title='Fan', title_key='Name', collapsable=True, ident='id_configuration_fan', attrs=[
                ce.String(key='Name', title='Name (single character, e.g. the same as corresponding extruder)'),
                pwm_output_choice(configuration_context, key='pwm_output', title='PWM output'),
                ce.Integer(key='SetMCommand', title='Set-command M-number (106 for first fan)'),
                ce.Integer(key='OffMCommand', title='Off-command M-number (107 for first fan)'),
            ])),
            ce.Compound('ProbeConfig', key='probe_config', title='Bed probing configuration', collapsable=True, attrs=[
                ce.OneOf(key='probe', title='Bed probing', choices=[
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
                        ce.Array(key='ProbePoints', title='Coordinates of probing points', table=True, elem=ce.Compound('ProbePoint', title='Point', attrs=[
                            ce.Float(key='X'),
                            ce.Float(key='Y'),
                        ]))
                    ])
                ])
            ]),
            ce.Array(key='lasers', title='Lasers', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('laser', title='Laser', title_key='Name', collapsable=True, ident='id_configuration_laser', attrs=[
                ce.String(key='Name', title='Name (single letter)', default='L'),
                ce.Reference(key='laser_port', title='Laser port', ref_array={'base': 'id_configuration.board_data', 'descend': ['laser_ports']}, ref_id_key='Name', ref_name_key='Name'),
                ce.String(key='DensityName', title='Density-control name (single letter)', default='M'),
                ce.Float(key='LaserPower', title='Laser power [Energy/s]', default=100),
                ce.Float(key='MaxPower', title='Maximum power [Energy/s] (values <LaserPower limit laser output)', default=100),
                ce.Float(key='AdjustmentInterval', title='Output adjustment interval [s]', default=0.005),
            ])),
        ])),
        ce.Array(key='boards', title='Boards', processing_order=-2, copy_name_key='name', elem=ce.Compound('board', title='Board', title_key='name', collapsable=True, ident='id_board', attrs=[
            ce.String(key='name', title='Name (modifying will break references from configurations and lose data)'),
            ce.Compound('PlatformConfig', key='platform_config', title='Platform configuration', collapsable=True, processing_order=-10, attrs=[
                ce.String(key='board_for_build', title='Board for building (see nix/boards.nix)'),
                ce.String(key='output_type', title='Build output type', enum=['hex', 'bin', 'elf']),
                ce.Array(key='board_helper_includes', title='Board helper includes', table=True, elem=ce.String(title='Name')),
                ce.OneOf(key='platform', title='Platform', processing_order=-1, choices=[
                    platform_At91Sam3x8e(),
                    platform_At91Sam3u4e(),
                    platform_Teensy3(),
                    platform_Avr('ATmega2560'),
                    platform_Avr('ATmega1284p'),
                    platform_Stm32f4(),
                ]),
            ]),
            pin_choice(key='LedPin', title='LED pin'),
            interrupt_timer_choice(key='EventChannelTimer', title='Event channel timer'),
            ce.Compound('RuntimeConfig', key='runtime_config', title='Runtime configuration', collapsable=True, attrs=[
                ce.OneOf(key='config_manager', title='Runtime configuration', choices=[
                    ce.Compound('ConstantConfigManager', title='Disabled', attrs=[]),
                    ce.Compound('RuntimeConfigManager', title='Enabled', attrs=[
                        ce.OneOf(key='ConfigStore', title='Configuration storage', choices=[
                            ce.Compound('NoStore', title='None', attrs=[]),
                            ce.Compound('EepromConfigStore', attrs=[
                                ce.Integer(key='StartBlock'),
                                ce.Integer(key='EndBlock'),
                                ce.OneOf(key='Eeprom', title='EEPROM backend', choices=[
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
                                    ce.Compound('FlashWrapper', attrs=[
                                        flash_choice(key='FlashDriver', title='Flash driver'),
                                    ]),
                                ]),
                            ])
                        ])
                    ]),
                ]),
            ]),
            ce.Compound('serial', key='serial', title='Serial parameters', collapsable=True, attrs=[
                ce.Integer(key='BaudRate', title='Baud rate'),
                ce.Integer(key='RecvBufferSizeExp', title='Receive buffer size (power of two exponent)'),
                ce.Integer(key='SendBufferSizeExp', title='Send buffer size (power of two exponent)'),
                ce.Integer(key='GcodeMaxParts', title='Max parts in GCode command'),
                ce.OneOf(key='Service', title='Backend', choices=[
                    ce.Compound('AsfUsbSerial', title='AT91 USB', attrs=[]),
                    ce.Compound('At91Sam3xSerial', title='AT91 UART', attrs=[]),
                    ce.Compound('TeensyUsbSerial', title='Teensy3 USB', attrs=[]),
                    ce.Compound('AvrSerial', title='AVR UART', attrs=[
                        ce.Boolean(key='DoubleSpeed'),
                    ]),
                    ce.Compound('Stm32f4UsbSerial', title='STM32F4 USB', attrs=[]),
                    ce.Compound('NullSerial', title='Null serial driver', attrs=[]),
                ])
            ]),
            ce.Compound('SdCardConfig', key='sdcard_config', title='SD card configuration', collapsable=True, attrs=[
                ce.OneOf(key='sdcard', title='SD card', choices=[
                    ce.Compound('NoSdCard', title='Disabled',attrs=[]),
                    ce.Compound('SdCard', title='Enabled', attrs=[
                        ce.OneOf(key='FsType', title='Filesystem type', choices=[
                            ce.Compound('Raw', title='None (raw data on device)', attrs=[]),
                            ce.Compound('Fat32', title='FAT32', attrs=[
                                ce.Integer(key='MaxFileNameSize', title='Maximum filename size', default=32),
                                ce.Integer(key='NumCacheEntries', title='Block cache size (in blocks)', default=2),
                                ce.Boolean(key='CaseInsensFileName', title='Case-insensitive filename matching', default=True),
                            ]),
                        ]),
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
                        ce.OneOf(key='SdCardService', title='Driver', choices=[
                            ce.Compound('SpiSdCard', title='SPI', attrs=[
                                pin_choice(key='SsPin', title='SS pin'),
                                spi_choice(key='SpiService', title='SPI driver')
                            ]),
                            ce.Compound('SdioSdCard', title='SDIO', attrs=[
                                sdio_choice(key='SdioService', title='SDIO driver'),
                            ]),
                        ])
                    ])
                ]),
            ]),
            ce.Compound('performance', key='performance', title='Performance parameters', collapsable=True, attrs=[
                ce.Float(key='MaxStepsPerCycle', title='Max steps per cycle'),
                ce.Integer(key='StepperSegmentBufferSize', title='Stepper segment buffer size'),
                ce.Integer(key='EventChannelBufferSize', title='Event channel buffer size'),
                ce.Integer(key='LookaheadBufferSize', title='Lookahead buffer size'),
                ce.Integer(key='LookaheadCommitCount', title='Lookahead commit count'),
                ce.String(key='FpType', enum=['float', 'double']),
                ce.String(key='AxisDriverPrecisionParams', title='Stepping precision parameters', enum=['AxisDriverAvrPrecisionParams', 'AxisDriverDuePrecisionParams']),
                ce.Float(key='EventChannelTimerClearance', title='Event channel timer clearance'),
                ce.Boolean(key='OptimizeForSize', title='Optimize compilation for program size', default=False),
            ]),
            ce.Compound('development', key='development', title='Development features', collapsable=True, attrs=[
                ce.Boolean(key='AssertionsEnabled', title='Enable assertions', default=False),
                ce.Boolean(key='EventLoopBenchmarkEnabled', title='Enable event-loop execution timing', default=False),
                ce.Boolean(key='DetectOverloadEnabled', title='Enable interrupt overload detection', default=False),
            ]),
            ce.Compound('CurrentConfig', key='current_config', title='Motor current control', collapsable=True, attrs=[
                ce.OneOf(key='current', title='Current control', choices=[
                    ce.Compound('NoCurrent', title='Disabled', attrs=[]),
                    ce.Compound('Current', title='Enabled', attrs=[
                        current_driver_choice(key='current_driver', title='Current control driver'),
                    ]),
                ]),
            ]),
            ce.Array(key='stepper_ports', title='Stepper ports', copy_name_key='Name', elem=ce.Compound('stepper_port', title='Stepper port', title_key='Name', collapsable=True, attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='DirPin', title='Direction pin'),
                pin_choice(key='StepPin', title='Step pin'),
                pin_choice(key='EnablePin', title='Enable pin'),
                ce.OneOf(key='StepperTimer', title='Stepper timer', choices=[
                    interrupt_timer_choice(title='Defined'),
                    ce.Compound('NoTimer', title='Not defined (for slave steppers only)', attrs=[]),
                ]),
                ce.OneOf(key='microstep', title='Micro-stepping', choices=[
                    ce.Compound('NoMicroStep', title='Not controlled (hardwired/none)', attrs=[]),
                    ce.Compound('MicroStep', title='Controlled', attrs=[
                        microstep_choice(key='MicroStepDriver', title='Stepper driver chip'),
                    ]),
                ]),
                ce.OneOf(key='current', title='Motor current control', choices=[
                    ce.Compound('NoCurrent', title='Not controlled', attrs=[]),
                    ce.Compound('Current', title='Controlled', attrs=[
                        current_driver_channel_choice(key='DriverChannelParams', title='Driver-specific parameters'),
                    ]),
                ]),
            ])),
            ce.Array(key='digital_inputs', title='Digital inputs', copy_name_key='Name', processing_order=-8, elem=ce.Compound('digital_input', title='Digital input', title_key='Name', collapsable=True, ident='id_board_digital_inputs', attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='Pin', title='Pin'),
                ce.Reference(key='InputMode', title='Input mode', ref_array={'base': 'id_board.platform_config.platform', 'descend': ['pins', 'input_modes']}, ref_id_key='ident', ref_name_key='name')
            ])),
            ce.Array(key='analog_inputs', title='Analog inputs', copy_name_key='Name', processing_order=-7, elem=ce.Compound('analog_input', title='Analog input', title_key='Name', collapsable=True, attrs=[
                ce.String(key='Name', title='Name'),
                pin_choice(key='Pin', title='Pin'),
            ])),
            ce.Array(key='pwm_outputs', title='PWM outputs', copy_name_key='Name', processing_order=-6, elem=ce.Compound('pwm_output', title='PWM output', title_key='Name', collapsable=True, attrs=[
                ce.String(key='Name', title='Name'),
                ce.OneOf(key='Backend', title='Backend', choices=[
                    ce.Compound('SoftPwm', attrs=[
                        pin_choice(key='OutputPin', title='Output pin'),
                        ce.Boolean(key='OutputInvert', title='Output logic', false_title='Normal (On=High)', true_title='Inverted (On=Low)'),
                        ce.Float(key='PulseInterval', title='PWM pulse duration'),
                        interrupt_timer_choice(key='Timer', title='Soft PWM Timer'),
                    ]),
                    ce.Compound('HardPwm', attrs=[
                        hard_pwm_choice(key='HardPwmDriver'),
                    ]),
                ])
            ])),
            ce.Array(key='laser_ports', title='Laser ports', copy_name_key='Name', elem=ce.Compound('laser_port', title='Laser port', title_key='Name', collapsable=True, ident='id_laser_port', attrs=[
                ce.String(key='Name', title='Name', default='Laser'),
                pwm_output_choice(board_context, key='pwm_output', title='PWM output (must be hard-PWM)'),
                interrupt_timer_choice(key='LaserTimer', title='Output adjustment timer'),
            ])),
        ]))
    ])
