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

def input_mode_choice(context, **kwargs):
    return ce.OneOf(choices=[
        ce.Compound('At91SamPinInputMode', attrs=[
            ce.String(key='PullMode', title='Pull mode', enum=['Normal', 'Pull-up']),
        ]),
        ce.Compound('Stm32f4PinInputMode', attrs=[
            ce.String(key='PullMode', title='Pull mode', enum=['Normal', 'Pull-up', 'Pull-down']),
        ]),
        ce.Compound('AvrPinInputMode', attrs=[
            ce.String(key='PullMode', title='Pull mode', enum=['Normal', 'Pull-up']),
        ]),
        ce.Compound('Mk20PinInputMode', attrs=[
            ce.String(key='PullMode', title='Pull mode', enum=['Normal', 'Pull-up', 'Pull-down']),
        ]),
        ce.Compound('StubPinInputMode', attrs=[]),
    ], **kwargs)

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
        ce.Compound('At91SamUsartSpi', attrs=[
            ce.Integer(key='DeviceIndex', default=0),
            ce.Integer(key='ClockDivider', default=420),
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
            ce.Integer(key='SdClockPrescaler', title='SD clock prescaler for transfer', default=0),
        ]),
        ce.Compound('At91SamSdio', attrs=[
            ce.Integer(key='Slot', title='Slot number', default=0),
            ce.Boolean(key='IsWideMode', title='Data bus width', false_title='1-bit', true_title='4-bit', default=False),
            ce.Integer(key='MaxIoDescriptors', title='Maximum number of buffers in transfer', default=20),
        ]),
    ], **kwargs)

def mii_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('At91SamEmacMii', attrs=[
            ce.Integer(key='NumRxBufers', title='Number of RX buffers [128 B]', default=48),
            ce.Integer(key='NumTxBufers', title='Number of TX buffers [frame]', default=4),
        ]),
    ], **kwargs)

def phy_choice(**kwargs):
    return ce.OneOf(choices=[
        ce.Compound('GenericPhy', attrs=[
            ce.Boolean(key='Rmii', title='Interface type', false_title='MII', true_title='RMII'),
            ce.Integer(key='PhyAddr', title='PHY address'),
        ]),
    ], **kwargs)

def heap_structure_choice(**kwargs):
    return ce.String(enum=['LinkedHeap', 'SortedList'], default='LinkedHeap', **kwargs)

def watchdog_at91sam():
    return ce.Compound('At91SamWatchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
        ce.Integer(key='Wdv', title='Wdv')
    ])

def pins_at91sam():
    return ce.Compound('At91SamPins', key='pins', title='Pins', collapsable=True, attrs=[
        ce.Constant(key='input_mode_type', value='At91SamPinInputMode'),
    ])

def at91sam_common():
    return [
        ce.Integer(key='AsfBoardNum', title='ASF board number (common/boards/board.h)'),
        ce.Integer(key='StackSize', title='Stack size', default=8192),
        ce.Integer(key='HeapSize', title='Heap size', default=16384),
    ]

def platform_At91Sam3x8e():
    return ce.Compound('At91Sam3x8e', attrs=(at91sam_common() + [
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
        ce.OneOf(key='fast_clock', title='Fast clock', choices=[
            ce.Compound('NoClock', title='Disabled', attrs=[]),
            ce.Compound('At91Sam3xClock', title='Enabled', attrs=[
                ce.Integer(key='prescaler', title='Prescaler'),
                ce.String(key='primary_timer', title='Timer'),
            ]),
        ]),
        ce.OneOf(key='millisecond_clock', title='Millisecond clock', choices=[
            ce.Compound('NoMillisecondClock', title='Disabled', attrs=[]),
            ce.Compound('ArmSysTickMillisecondClock', title='ARM SysTick', attrs=[]),
        ]),
    ]))

def platform_At91Sam3u4e():
    return ce.Compound('At91Sam3u4e', attrs=(at91sam_common() + [
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
    ]))

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
        ce.OneOf(key='fast_clock', title='Fast clock', choices=[
            ce.Compound('NoClock', title='Disabled', attrs=[]),
            ce.Compound('Mk20Clock', title='Enabled', attrs=[
                ce.Integer(key='prescaler', title='Prescaler'),
                ce.String(key='primary_timer', title='Timer'),
            ]),
        ]),
        ce.Compound('Mk20Adc', key='adc', title='ADC', collapsable=True, attrs=[
            ce.Integer(key='AdcADiv', title='AdcADiv'),
        ]),
        ce.Compound('Mk20Watchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
            ce.Integer(key='Toval', title='Timeout value'),
            ce.Integer(key='Prescval', title='Prescaler value'),
        ]),
        ce.Compound('Mk20Pins', key='pins', title='Pins', collapsable=True, attrs=[
            ce.Constant(key='input_mode_type', value='Mk20PinInputMode'),
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
        ce.Integer(key='CpuFreq', title='CPU frequency [Hz]'),
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
            ce.Integer(key='OverSamplingBits', default=0),
        ]),
        ce.Compound('AvrWatchdog', key='watchdog', title='Watchdog', collapsable=True, attrs=[
            ce.String(key='Timeout', title='Timeout (WDTO_*)'),
        ]),
        ce.Compound('AvrPins', key='pins', title='Pins', collapsable=True, attrs=[
            ce.Constant(key='input_mode_type', value='AvrPinInputMode'),
        ]),
    ])

def platform_stm32f4_generic(platform_name):
    return ce.Compound(platform_name, attrs=[
        ce.Integer(key='HeapSize', title='Heap size', default=16384),
        ce.Integer(key='HSE_VALUE'),
        ce.Integer(key='PLL_N_VALUE'),
        ce.Integer(key='PLL_M_VALUE'),
        ce.Integer(key='PLL_P_DIV_VALUE'),
        ce.Integer(key='PLL_Q_DIV_VALUE'),
        ce.Integer(key='APB1_PRESC_DIV'),
        ce.Integer(key='APB2_PRESC_DIV'),
        ce.String(key='UsbMode', title='USB mode (if using USB serial)', enum=['None', 'FS', 'HS', 'HS-in-FS']),
        ce.Compound('Stm32f4Clock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='prescaler', title='Prescaler'),
            ce.String(key='primary_timer', title='Primary timer'),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': 'TIM{}_{}'.format(n, m)
                } for n in [2,3,4,5,12,13,14] for m in [1,2,3,4]
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
            ce.Constant(key='input_mode_type', value='Stm32f4PinInputMode'),
        ]),
        ce.OneOf(key='fast_clock', title='Fast clock', choices=[
            ce.Compound('NoClock', title='Disabled', attrs=[]),
            ce.Compound('Stm32f4Clock', key='clock', title='Enabled', attrs=[
                ce.Integer(key='prescaler', title='Prescaler'),
                ce.String(key='primary_timer', title='Timer'),
            ]),
        ]),
    ])

def platform_Linux():
    return ce.Compound('Linux', attrs=[
        ce.Compound('LinuxClock', key='clock', title='Clock', collapsable=True, attrs=[
            ce.Integer(key='SubSecondBits', title='Sub-second time bits (clock precision)', default=21),
            ce.Integer(key='MaxTimers', title='Maximum number of timers', default=10),
            ce.Constant(key='primary_timer', value=''),
            ce.Constant(key='avail_oc_units', value=[
                {
                    'value': '{}'.format(n)
                } for n in range(16)
            ]),
        ]),
        ce.Compound('NoAdc', key='adc', title='ADC', attrs=[]),
        ce.Compound('NullWatchdog', key='watchdog', title='Watchdog', attrs=[]),
        ce.Compound('StubPins', key='pins', title='Pins', attrs=[
            ce.Constant(key='input_mode_type', value='StubPinInputMode'),
        ]),
        heap_structure_choice(key='TimersStructure', title='Data structure for timers'),
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
            ce.Boolean(key='Invert', title='Output logic', false_title='Normal (On=High)', true_title='Inverted (On=Low)'),
        ]),
    ], **kwargs)

def stepper_homing_params(**kwargs):
    return ce.OneOf(title='Homing', choices=[
        ce.Compound('no_homing', title='Disabled', attrs=[]),
        ce.Compound('homing', title='Enabled', ident='id_board_steppers_homing', attrs=[
            ce.Boolean(key='HomeDir', title='Homing direction', false_title='Negative', true_title='Positive', default=False),
            ce.Float(key='HomeOffset', title='Offset of home position from min/max position [mm]', default=0),
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

def virtual_homing_params(**kwargs):
    return ce.OneOf(title='Homing', choices=[
        ce.Compound('no_homing', title='Disabled', attrs=[]),
        ce.Compound('homing', title='Enabled', ident='id_board_steppers_homing', attrs=[
            ce.Boolean(key='ByDefault', title='Enabled by default (in plain G28)', default=True),
            ce.Boolean(key='HomeDir', title='Homing direction', false_title='Negative', true_title='Positive', default=False),
            digital_input_choice(key='HomeEndstopInput', title='Endstop digital input'),
            ce.Boolean(key='HomeEndInvert', title='Invert endstop', false_title='No (high signal is pressed)', true_title='Yes (low signal is pressed)', default=False),
            ce.Float(key='HomeFastExtraDist', title='Extra permitted travel for fast move [mm] (on top of abs(MaxPos-MinPos))', default=10),
            ce.Float(key='HomeRetractDist', title='Retraction travel [mm]', default=3),
            ce.Float(key='HomeSlowExtraDist', title='Extra permitted travel for slow move [mm]', default=5),
            ce.Float(key='HomeFastSpeed', title='Fast speed [mm/s]', default=40),
            ce.Float(key='HomeRetractSpeed', title='Retraction speed [mm/s]', default=50),
            ce.Float(key='HomeSlowSpeed', title='Slow speed [mm/s]', default=5)
        ])
    ], **kwargs)

def make_transform_type(transform_type, transform_title, stepper_defs, axis_defs, specific_params):
    assert len(stepper_defs) == len(axis_defs)
    
    return ce.Compound(transform_type, title=transform_title, attrs=(
        specific_params +
        [
            ce.OneOf(key='Splitter', title='Segmentation', choices=[
                ce.Compound('DistanceSplitter', title='Enabled', attrs=[
                    ce.Float(key='MinSplitLength', title='Minimum segment length [mm]', default=0.1),
                    ce.Float(key='MaxSplitLength', title='Maximum segment length [mm]', default=4.0),
                    ce.Float(key='SegmentsPerSecond', title='Segments per second', default=100.0),
                ]),
                ce.Compound('NoSplitter', title='Disabled', attrs=[]),
            ]),
        ] +
        [
            ce.Constant(key='DimensionCount', value=len(stepper_defs)),
        ] +
        ([
            ce.Constant(key='Steppers', value={}),
            ce.Constant(key='CartesianAxes', value={}),
        ] if len(stepper_defs) == 0 else [
            ce.Compound('Steppers', key='Steppers', title='Stepper mapping', attrs=[
                ce.Compound('TransformStepperParams', key='TransformStepper{}'.format(i), title=stepper_def['title'], collapsable=True, attrs=[
                    ce.String(key='StepperName', title='Name of stepper to use', default=stepper_def['default_name']),
                ])
                for (i, stepper_def) in enumerate(stepper_defs)
            ]),
            ce.Compound('CartesianAxes', key='CartesianAxes', title='Cartesian axes', attrs=[
                ce.Compound('VirtualAxisParams', key='VirtualAxis{}'.format(i), title='Cartesian axis / role {}'.format(axis_def['axis_name']), collapsable=True, attrs=(
                    [
                        ce.String(key='Name', default=axis_def['axis_name']),
                        ce.Float(key='MinPos', title='Minimum position [mm]', default=0),
                        ce.Float(key='MaxPos', title='Maximum position [mm]', default=200),
                        ce.Float(key='MaxSpeed', title='Maximum speed [mm/s]', default=300),
                    ] +
                    (
                        [virtual_homing_params(key='homing')] if axis_def['homing_allowed'] else
                        [ce.Constant(key='homing', value={'_compoundName': 'no_homing'})]
                    )
                ))
                for (i, axis_def) in enumerate(axis_defs)
            ]),
        ]) +
        [
            ce.Array(key='IdentityAxes', title='Extra identity axes', elem=ce.Compound('IdentityAxis', title='Identity axis', title_key='Name', collapsable=True, attrs=[
                ce.String(key='Name', title='Name'),
                ce.String(key='StepperName', title='Stepper name'),
                ce.OneOf(key='Limits', title='Position limits', choices=[
                    ce.Compound('LimitsAsStepper', title='Same as stepper', attrs=[]),
                    ce.Compound('LimitsSpecified', title='Specified', attrs=[
                        ce.Float(key='MinPos', title='Minimum position [mm]', default=-1000),
                        ce.Float(key='MaxPos', title='Maximum position [mm]', default=1000),
                    ]),
                ]),
            ])),
        ]
    ))

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
        ce.Array(key='configurations', title='Configurations', processing_order=-1, copy_name_key='name', elem=ce.Compound('config', key='config', ident='id_configuration', title='Configuration', title_key='name', collapsable=True, attrs=[
            ce.String(key='name', title='Configuration name', default='New Configuration'),
            ce.Reference(key='board', ref_array={'base': 'id_editor.boards', 'descend': []}, ref_id_key='name', ref_name_key='name', deref_key='board_data', title='Board', processing_order=-1),
            ce.Float(key='InactiveTime', title='Disable steppers after [s]', default=480),
            ce.Float(key='WaitTimeout', title='Timeout when waiting for heater temperatures (M116) [s]', default=500),
            ce.Float(key='WaitReportPeriod', title='Period of temperature reports when waiting for heaters [s]', default=1),
            ce.Compound('advanced', key='advanced', title='Advanced parameters', collapsable=True, attrs=[
                ce.Float(key='LedBlinkInterval', title='LED blink interval [s]', default=0.5),
                ce.Float(key='ForceTimeout', title='Force motion timeout [s]', default=0.1),
            ]),
            ce.Array(key='steppers', title='Axes', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('stepper', title='Axis', title_key='Name', collapsable=True, ident='id_configuration_stepper', attrs=[
                ce.String(key='Name', title='Name (cartesian X/Y/Z, extruders E/U/V, delta A/B/C)'),
                ce.Array(key='slave_steppers', title='Steppers', elem=ce.Compound('slave_stepper', ident='id_slave_stepper', title='Stepper', attrs=[
                    ce.Reference(key='stepper_port', title='Stepper port', ref_array=configuration_context.board_ref(['stepper_ports']), ref_id_key='Name', ref_name_key='Name'),
                    ce.Boolean(key='InvertDir', title='Invert direction', default=False),
                    ce.Integer(key='MicroSteps', title='Micro-steps (if board supports micro-step configuration)', default=0),
                    ce.Float(key='Current', title='Motor current (if board supports current control) [mA]', default=0),
                ])),
                ce.Float(key='StepsPerUnit', title='Steps per unit [1/mm]', default=80),
                ce.Float(key='MinPos', title='Minimum position [mm] (~-40000 for extruders)', default=0),
                ce.Float(key='MaxPos', title='Maximum position [mm] (~40000 for extruders)', default=200),
                ce.Float(key='MaxSpeed', title='Maximum speed [mm/s]', default=300),
                ce.Float(key='MaxAccel', title='Maximum acceleration [mm/s^2]', default=1500),
                ce.Float(key='DistanceFactor', title='Distance factor [1]', default=1),
                ce.Float(key='CorneringDistance', title='Cornering distance (greater values allow greater change of speed at corners) [step]', default=40),
                ce.Boolean(key='EnableCartesianSpeedLimit', title='Is cartesian (Yes for X/Y/Z, No for extruders)', default=True),
                ce.Boolean(key='IsExtruder', title='Is an extruder (e.g. subject to M82/M83)', default=False),
                stepper_homing_params(key='homing'),
                ce.Boolean(key='PreloadCommands', title='Command loading mode', default=False, false_title='At first step', true_title='At last step of previous command (use when direction-ahead-of-step-time is large)'),
                ce.OneOf(key='delay', title='Step signals timing', choices=[
                    ce.Compound('NoDelay', title='No special delays', attrs=[]),
                    ce.Compound('Delay', title='Use delays to ensure required timing', attrs=[
                        ce.Float(key='DirSetTime', title='Minimum direction ahead of step time [us]', default=0.2),
                        ce.Float(key='StepHighTime', title='Minimum step high time [us]', default=1.0),
                        ce.Float(key='StepLowTime', title='Minimum step low time [us]', default=1.0),
                    ]),
                ]),
            ])),
            ce.OneOf(key='transform', title='Coordinate transformation', choices=[
                ce.Compound('NoTransform', title='None (cartesian)', attrs=[]),
                make_transform_type(transform_type='Null', transform_title='Identity',
                    stepper_defs=[],
                    axis_defs=[],
                    specific_params=[]
                ),
                make_transform_type(transform_type='CoreXY', transform_title='CoreXY/H-bot',
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
                make_transform_type(transform_type='Delta', transform_title='Delta',
                    stepper_defs=[
                        {'default_name': 'A', 'title': 'Tower-1 stepper (bottom-left)'},
                        {'default_name': 'B', 'title': 'Tower-2 stepper (bottom-right)'},
                        {'default_name': 'C', 'title': 'Tower-3 stepper (top)'},
                    ],
                    axis_defs=[
                        {'axis_name': 'X', 'homing_allowed': False},
                        {'axis_name': 'Y', 'homing_allowed': False},
                        {'axis_name': 'Z', 'homing_allowed': True},
                    ],
                    specific_params=[
                        # TODO: Fix typo in DiagnalRod
                        ce.Float(key='DiagnalRod', title='Diagonal rod length [mm]', default=214),
                        ce.Float(key='DiagonalRodCorr1', title='Diagonal rod length correction for Tower-1 [mm]', default=0),
                        ce.Float(key='DiagonalRodCorr2', title='Diagonal rod length correction for Tower-2 [mm]', default=0),
                        ce.Float(key='DiagonalRodCorr3', title='Diagonal rod length correction for Tower-3 [mm]', default=0),
                        ce.Float(key='SmoothRodOffset', title='Smooth rod offset [mm]', default=145),
                        ce.Float(key='EffectorOffset', title='Effector offset [mm]', default=19.9),
                        ce.Float(key='CarriageOffset', title='Carriage offset [mm]', default=19.5),
                        ce.Float(key='LimitRadius', title='Radius of XY disk to permit motion in [mm]', default=150.0),
                    ]
                ),
                make_transform_type(transform_type='RotationalDelta', transform_title='Rotational delta',
                    stepper_defs=[
                        {'default_name': 'A', 'title': 'Arm-1 stepper (bottom)'},
                        {'default_name': 'B', 'title': 'Arm-2 stepper (top-right)'},
                        {'default_name': 'C', 'title': 'Arm-3 stepper (top-left)'},
                    ],
                    axis_defs=[
                        {'axis_name': 'X', 'homing_allowed': False},
                        {'axis_name': 'Y', 'homing_allowed': False},
                        {'axis_name': 'Z', 'homing_allowed': False},
                    ],
                    specific_params=[
                        ce.Float(key='EndEffectorLength', title='End effector length [mm]', default=30.0),
                        ce.Float(key='BaseLength', title='Base length [mm]', default=40.0),
                        ce.Float(key='RodLength', title='Rod length [mm]', default=130.0),
                        ce.Float(key='ArmLength', title='Arm length [mm]', default=80.0),
                        ce.Float(key='ZOffset', title='Z offset [mm]', default=200.0),
                    ]
                ),
                make_transform_type(transform_type='SCARA', transform_title='SCARA, single arm',
                    stepper_defs=[
                        {'default_name': 'A', 'title': 'Shoulder stepper (Arm1)'},
                        {'default_name': 'B', 'title': 'Elbow stepper (Arm2)'},
                    ],
                    axis_defs=[
                        {'axis_name': 'X', 'homing_allowed': False},
                        {'axis_name': 'Y', 'homing_allowed': False},
                    ],
                    specific_params=[
                        ce.Float(key='Arm1Length', title='Length of first arm [mm]', default=150.0),
                        ce.Float(key='Arm2Length', title='Length of second arm [mm]', default=150.0),
                        ce.Boolean(key='ExternalArm2Motor', title='Is the driving motor of the second arm external (i.e. not built into arm1)', default=True),
                        ce.Float(key='XOffset', title='X offset [mm]', default=0.0),
                        ce.Float(key='YOffset', title='Y offset [mm]', default=0.0),
                    ]
                ),
                make_transform_type(transform_type='DualSCARA', transform_title='SCARA, two arms',
                    stepper_defs=[
                        {'default_name': 'A', 'title': 'Left shoulder stepper (Arm1)'},
                        {'default_name': 'B', 'title': 'Right shoulder stepper (Arm2)'},
                    ],
                    axis_defs=[
                        {'axis_name': 'X', 'homing_allowed': False},
                        {'axis_name': 'Y', 'homing_allowed': False},
                    ],
                    specific_params=[
                        ce.Float(key='Arm1ShoulderXCoord', title='X coordinate of left shoulder (probably negative) [mm]', default=-50.0),
                        ce.Float(key='Arm2ShoulderXCoord', title='X coordinate of right shoulder (probably positive) [mm]', default=50.0),
                        ce.Float(key='Arm1ProximalSideLength', title='Length of proximal segment of left arm [mm]', default=60.0),
                        ce.Float(key='Arm2ProximalSideLength', title='Length of proximal segment of right arm [mm]', default=60.0),
                        ce.Float(key='Arm1DistalSideLength', title='Length of distal segment of left arm [mm]', default=70.0),
                        ce.Float(key='Arm2DistalSideLength', title='Length of distal segment of right arm [mm]', default=70.0),
                        ce.Float(key='XOffset', title='X offset [mm]', default=0.0),
                        ce.Float(key='YOffset', title='Y offset [mm]', default=0.0),
                    ]
                ),
            ]),
            ce.Array(key='heaters', title='Heaters', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('heater', title='Heater', title_key='Name', collapsable=True, ident='id_configuration_heater', attrs=[
                ce.String(key='Name', title='Name (capital letter optionally followed by a number; T=extruder, B=bed)'),
                ce.Integer(key='SetMCommand', title='Set command M-number (optional; M104 can set any heater)', default=0),
                ce.Integer(key='SetWaitMCommand', title='Set-and-wait command M-number (optional; M109 can set any heater)', default=0),
                pwm_output_choice(configuration_context, key='pwm_output', title='PWM output'),
                analog_input_choice(key='ThermistorInput', title='Thermistor analog input'),
                ce.Float(key='MinSafeTemp', title='Turn off if temperature is below [C]', default=10),
                ce.Float(key='MaxSafeTemp', title='Turn off if temperature is above [C]', default=280),
                ce.OneOf(key='conversion', title='Temperature conversion', choices=[
                    ce.Compound('conversion', title='Generic thermistor', attrs=[
                        ce.Float(key='ResistorR', title='Series-resistor resistance [ohm]', default=4700),
                        ce.Float(key='R0', title='Thermistor resistance @25C [ohm]', default=100000),
                        ce.Float(key='Beta', title='Thermistor beta value [K]', default=3960),
                        ce.Float(key='MinTemp', title='Reliable measurements are above [C]', default=10),
                        ce.Float(key='MaxTemp', title='Reliable measurements are below [C]', default=300)
                    ]),
                    ce.Compound('PtRtdFormula', title='Platinum resistance thermometer (PRT)', attrs=[
                        ce.Float(key='ResistorR', title='Series-resistor resistance [ohm]', default=4700),
                        ce.Float(key='PtR0', title='Resistance @0C [ohm]', default=1000),
                        ce.Float(key='PtA', title='A (linear factor)', default=3.90830E-3),
                        ce.Float(key='PtB', title='B (quadratic factor)', default=5.7750E-7),
                        ce.Float(key='MinTemp', title='Reliable measurements are above [C]', default=0),
                        ce.Float(key='MaxTemp', title='Reliable measurements are below [C]', default=600)
                    ]),
                    ce.Compound('Max31855Formula', title='MAX31855 conversion', attrs=[]),
                    ce.Compound('E3dPt100', title='E3D PT100 Amplifier', attrs=[]),
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
                ]),
                ce.OneOf(key='cold_extrusion_prevention', title='Cold-extrusion prevention', choices=[
                    ce.Compound('NoColdExtrusionPrevention', title='Disabled', attrs=[]),
                    ce.Compound('ColdExtrusionPrevention', title='Enabled', attrs=[
                        ce.Float(key='MinExtrusionTemp', title='Minimum temperature for extrusion [C]', default=200),
                        ce.Array(key='ExtruderAxes', title='Extruder axes', table=True, elem=ce.String(key='AxisName', title='Axis name')),
                    ]),
                ]),
            ])),
            ce.Array(key='fans', title='Fans', copy_name_key='Name', copy_name_suffix='?', elem=ce.Compound('fan', title='Fan', title_key='Name', collapsable=True, ident='id_configuration_fan', attrs=[
                ce.String(key='Name', title='Name (capital letter optionally followed by a number)'),
                pwm_output_choice(configuration_context, key='pwm_output', title='PWM output'),
                ce.Integer(key='SetMCommand', title='Set-command M-number (optional; M106 can set any fan)'),
                ce.Integer(key='OffMCommand', title='Off-command M-number (optional; M107 can off any fan)'),
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
                        ce.Float(key='GeneralZOffset', title='Z offset added to height measurements [mm] (increase for correction to raise nozzle)', default=0),
                        ce.Array(key='ProbePoints', title='Coordinates of probing points', table=True, elem=ce.Compound('ProbePoint', title='Point', attrs=[
                            ce.Boolean(key='Enabled', default=True),
                            ce.Float(key='X'),
                            ce.Float(key='Y'),
                            ce.Float(key='Z-offset'),
                        ])),
                        ce.OneOf(key='correction', title='Bed correction', choices=[
                            ce.Compound('NoCorrection', title='Disabled', attrs=[]),
                            ce.Compound('Correction', title='Enabled', attrs=[
                                ce.Boolean(key='QuadraticCorrectionSupported', title='Support quadratic correction', default=False),
                                ce.Boolean(key='QuadraticCorrectionEnabled', title='Enable quadratic correction', default=False),
                            ]),
                        ])
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
            ce.OneOf(key='Moves', title='Predefined moves', choices=[
                ce.Compound('NoMoves', title='Disabled', attrs=[]),
                ce.Compound('Moves', title='Enabled', attrs=[
                    ce.Array(key='Moves', title='Moves', elem=ce.Compound('Move', title='Move', collapsable=True, attrs=[
                        ce.String(key='HookType', title='Upon event', enum=['After homing', 'After bed probing']),
                        ce.Integer(key='HookPriority', title='Priority', default=10),
                        ce.Boolean(key='Enabled', title='Enabled', default=True),
                        ce.Float(key='Speed', title='Speed [mm/s]', default=200),
                        ce.Array(key='Coordinates', title='Coordinates', table=True, elem=ce.Compound('Coordinate', title='Coordinate', attrs=[
                            ce.String(key='AxisName', title='Axis', default='X'),
                            ce.Float(key='Value', title='Value', default=100),
                        ])),
                    ])),
                ]),
            ]),
        ])),
        ce.Array(key='boards', title='Boards', processing_order=-2, copy_name_key='name', elem=ce.Compound('board', title='Board', title_key='name', collapsable=True, ident='id_board', attrs=[
            ce.String(key='name', title='Name (modifying will break references from configurations and lose data)'),
            ce.Compound('PlatformConfig', key='platform_config', title='Platform configuration', collapsable=True, processing_order=-10, attrs=[
                ce.Compound('output_types', key='output_types', title='Binary outputs types', attrs=[
                    ce.Boolean(key='output_elf', title='.elf'),
                    ce.Boolean(key='output_bin', title='.bin'),
                    ce.Boolean(key='output_hex', title='.hex'),
                ]),
                ce.Array(key='board_helper_includes', title='Board helper includes', table=True, elem=ce.String(title='Name')),
                ce.OneOf(key='platform', title='Platform', processing_order=-1, choices=[
                    platform_At91Sam3x8e(),
                    platform_At91Sam3u4e(),
                    platform_Teensy3(),
                    platform_Avr('ATmega2560'),
                    platform_Avr('ATmega1284p'),
                    platform_stm32f4_generic('stm32f407'),
                    platform_stm32f4_generic('stm32f411'),
                    platform_stm32f4_generic('stm32f429'),
                    platform_Linux(),
                ]),
                ce.OneOf(key='debug_interface', title='Debug interface', choices=[
                    ce.Compound('NoDebug', title='None or specified elsewhere', attrs=[]),
                    ce.Compound('ArmItmDebug', title='ARM ITM', attrs=[
                        ce.Integer(key='StimulusPort', title='Stimulus port number'),
                    ]),
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
                            ]),
                            ce.Compound('FileConfigStore', title='File on SD card', attrs=[]),
                        ])
                    ]),
                ]),
            ]),
            ce.Array(key='serial_ports', title='Serial ports', elem=ce.Compound('serial', title='Serial port', collapsable=True, attrs=[
                ce.Integer(key='BaudRate', title='Baud rate'),
                ce.Integer(key='RecvBufferSizeExp', title='Receive buffer size (power of two exponent)'),
                ce.Integer(key='SendBufferSizeExp', title='Send buffer size (power of two exponent)'),
                ce.Integer(key='GcodeMaxParts', title='Max parts in GCode command'),
                ce.OneOf(key='Service', title='Backend', choices=[
                    ce.Compound('AsfUsbSerial', title='AT91 USB', attrs=[]),
                    ce.Compound('At91Sam3xSerial', title='AT91 UART', attrs=[
                        ce.Boolean(key='UseForDebug', title='Use for debug output', default=True),
                    ]),
                    ce.Compound('TeensyUsbSerial', title='Teensy3 USB', attrs=[]),
                    ce.Compound('AvrSerial', title='AVR UART', attrs=[
                        ce.Boolean(key='DoubleSpeed'),
                    ]),
                    ce.Compound('Stm32f4UsbSerial', title='STM32F4 USB', attrs=[]),
                    ce.Compound('LinuxStdInOutSerial', title='Linux stdin/stdout', attrs=[]),
                    ce.Compound('NullSerial', title='Null serial driver', attrs=[]),
                ])
            ])),
            ce.Compound('SdCardConfig', key='sdcard_config', title='SD card configuration', collapsable=True, attrs=[
                ce.OneOf(key='sdcard', title='SD card', choices=[
                    ce.Compound('NoSdCard', title='Disabled',attrs=[]),
                    ce.Compound('SdCard', title='Enabled', attrs=[
                        ce.OneOf(key='FsType', title='Filesystem type', choices=[
                            ce.Compound('Raw', title='None (raw data on device)', attrs=[]),
                            ce.Compound('Fat32', title='FAT32', attrs=[
                                ce.Integer(key='MaxFileNameSize', title='Maximum filename size', default=32),
                                ce.Integer(key='NumCacheEntries', title='Block cache size (in blocks)', default=2),
                                ce.Integer(key='MaxIoBlocks', title='Maximum blocks in single I/O command', default=1),
                                ce.Boolean(key='CaseInsensFileName', title='Case-insensitive filename matching', default=True),
                                ce.Boolean(key='FsWritable', title='Writable filesystem', default=False),
                                ce.Boolean(key='EnableReadHinting', title='Enable read-ahead hinting', default=False),
                                ce.Boolean(key='HaveAccessInterface', title='Enable internal FS access interface', default=False),
                                ce.Boolean(key='EnableFsTest', title='Enable FS test module', default=False),
                                ce.OneOf(key='GcodeUpload', title='G-code upload', choices=[
                                    ce.Compound('NoGcodeUpload', title='Disabled', attrs=[]),
                                    ce.Compound('GcodeUpload', title='Enabled', attrs=[
                                        ce.Integer(key='MaxCommandSize', title='Maximum command size', default=128),
                                    ]),
                                ]),
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
                            ce.Compound('LinuxSdCard', title='Linux file/device', attrs=[
                                ce.Integer(key='BlockSize', default=512),
                                ce.Integer(key='MaxIoBlocks', default=1024),
                                ce.Integer(key='MaxIoDescriptors', default=32),
                            ]),
                        ])
                    ])
                ]),
            ]),
            ce.Compound('NetworkConfig', key='network_config', title='Network configuration', collapsable=True, attrs=[
                ce.OneOf(key='network', title='Network', choices=[
                    ce.Compound('NoNetwork', title='Disabled', attrs=[]),
                    ce.Compound('Network', title='Enabled', attrs=[
                        ce.OneOf(key='EthernetDriver', title='Ethernet driver', choices=[
                            ce.Compound('MiiEthernet', title='MII-based', attrs=[
                                mii_choice(key='MiiDriver', title='MII driver'),
                                phy_choice(key='PhyDriver', title='PHY driver')
                            ]),
                            ce.Compound('LinuxTapEthernet', title='Linux TAP', attrs=[]),
                        ]),
                        ce.Integer(key='NumArpEntries', title='Number of ARP entries', default=16),
                        ce.Integer(key='ArpProtectCount', title='Number of protected ARP entries', default=8),
                        ce.Integer(key='MaxReassPackets', title='Max packets in reassembly', default=1),
                        ce.Integer(key='MaxReassSize', title='Max reassembled data size', default=1480),
                        ce.Integer(key='MaxReassHoles', title='Max holes in packet being reassembled', default=10),
                        ce.Integer(key='MaxReassTimeSeconds', title='Max packet reassembly timeout', default=60),
                        ce.Integer(key='MtuTimeoutMinutes', title='MTU information timeout [min]', default=10),
                        ce.String(key='MtuIndexService', title='Data structure for MTU information', enum=['MruListIndex', 'AvlTreeIndex'], default='MruListIndex'),
                        ce.Integer(key='NumTcpPcbs', title='Number of TCP PCBs', default=16),
                        ce.Integer(key='NumOosSegs', title='Number of out-of-sequence segment slots', default=4),
                        ce.Integer(key='TcpWndUpdThrDiv', title='TCP window update threshold divisor (threshold = RX buffer / N)', default=4),
                        ce.String(key='PcbIndexService', title='Data structure for looking up PCBs', enum=['MruListIndex', 'AvlTreeIndex'], default='MruListIndex'),
                        ce.Boolean(key='LinkWithArrayIndices', title='Use array indices in data structure', default=True),
                        heap_structure_choice(key='ArpTableTimersStructureService', title='Data structure for ARP table entry timers'),
                        ce.Boolean(key='NetEnabled', title='Networking enabled', default=True),
                        ce.String(key='MacAddress', title='MAC address', default='BE:EF:DE:AD:FE:ED'),
                        ce.Boolean(key='DhcpEnabled', title='DHCP enabled', default=True),
                        ce.String(key='IpAddress', title='IP address (when DHCP is disabled)', default='0.0.0.0'),
                        ce.String(key='IpNetmask', title='Network mask (when DHCP is disabled)', default='0.0.0.0'),
                        ce.String(key='IpGateway', title='Default gateway (when DHCP is disabled)', default='0.0.0.0'),
                        ce.OneOf(key='tcpconsole', title='TCP console', choices=[
                            ce.Compound('NoTcpConsole', title='Disabled', attrs=[]),
                            ce.Compound('TcpConsole', title='Enabled', attrs=[
                                ce.Integer(key='Port', title='Console port number', default=23),
                                ce.Integer(key='MaxClients', title='Maximum number of clients', default=2),
                                ce.Integer(key='MaxPcbs', title='Maximum number of PCBs', default=4),
                                ce.Integer(key='MaxParts', title='Max parts in GCode command', default=16),
                                ce.Integer(key='MaxCommandSize', title='Maximum command size', default=128),
                                ce.Integer(key='SendBufferSize', title='Send buffer size [bytes]', default=3*1460),
                                ce.Integer(key='RecvBufferSize', title='Receive buffer size [bytes]', default=2*1460),
                                ce.Float(key='SendBufTimeout', title='Timeout when waiting for send buffer space [s]', default=5.0),
                                ce.Float(key='SendEndTimeout', title='Timeout when waiting to send remaining data [s]', default=10.0),
                            ]),
                        ]),
                        ce.OneOf(key='webinterface', title='Web interface', choices=[
                            ce.Compound('NoWebInterface', title='Disabled', attrs=[]),
                            ce.Compound('WebInterface', title='Enabled', attrs=[
                                ce.Integer(key='Port', title='HTTP port number', default=80),
                                ce.Integer(key='MaxClients', title='Maximum number of active clients', default=2),
                                ce.Integer(key='MaxPcbs', title='Maximum number of PCBs', default=10),
                                ce.Integer(key='QueueSize', title='Maximum number of queued clients', default=6),
                                ce.Integer(key='QueueRecvBufferSize', title='Receive buffer size for queued clients [bytes]', default=410),
                                ce.Integer(key='SendBufferSize', title='Send buffer size [bytes]', default=3*1460),
                                ce.Integer(key='RecvBufferSize', title='Receive buffer size [bytes]', default=2*1460),
                                ce.Boolean(key='AllowPersistent', title='Allow persistent connections', default=False),
                                ce.Float(key='QueueTimeout', title='Timeout for queued clients [s]', default=10),
                                ce.Float(key='InactivityTimeout', title='Network inactivity timeout [s]', default=10),
                                ce.Boolean(key='EnableDebug', title='Enable debug messages', default=False),
                                ce.Integer(key='JsonBufferSize', title='Size of JSON response buffer', default=400),
                                ce.Integer(key='NumGcodeSlots', title='Maximum simultaneous g-code sessions', default=1),
                                ce.Integer(key='MaxGcodeParts', title='Max parts in g-code command', default=16),
                                ce.Integer(key='MaxGcodeCommandSize', title='Maximum g-code command size', default=128),
                                ce.Float(key='GcodeSendBufTimeout', title='Timeout when waiting for send buffer space for g-code commands [s]', default=5.0),
                            ]),
                        ]),
                    ])
                ]),
            ]),
            ce.Compound('performance', key='performance', title='Performance parameters', collapsable=True, attrs=[
                ce.Integer(key='ExpectedResponseLength', title='Maximum expected length of command responses', default=60),
                ce.Integer(key='ExtraSendBufClearance', title='Extra space to reserve in send buffers (for status messages)', default=60),
                ce.Integer(key='MaxMsgSize', title='Maximum length of status messages', default=60),
                ce.Float(key='MaxStepsPerCycle', title='Max steps per cycle'),
                ce.Integer(key='StepperSegmentBufferSize', title='Stepper segment buffer size'),
                ce.Integer(key='EventChannelBufferSize', title='Event channel buffer size'),
                ce.Integer(key='LookaheadBufferSize', title='Lookahead buffer size'),
                ce.Integer(key='LookaheadCommitCount', title='Lookahead commit count'),
                ce.String(key='FpType', enum=['float', 'double']),
                ce.String(key='AxisDriverPrecisionParams', title='Stepping precision parameters', enum=['AxisDriverAvrPrecisionParams', 'AxisDriverDuePrecisionParams']),
                ce.Float(key='EventChannelTimerClearance', title='Event channel timer clearance'),
                ce.Boolean(key='OptimizeForSize', title='Optimize compilation for program size', default=False),
                ce.Boolean(key='OptimizeLibcForSize', title='Optimize libc for program size (newlib/ARM only)', default=False),
            ]),
            ce.Compound('development', key='development', title='Development features', collapsable=True, attrs=[
                ce.Boolean(key='AssertionsEnabled', title='Enable assertions', default=False),
                ce.Boolean(key='EventLoopBenchmarkEnabled', title='Enable event-loop execution timing', default=False),
                ce.Boolean(key='DetectOverloadEnabled', title='Enable interrupt overload detection', default=False),
                ce.Boolean(key='WatchdogDebugMode', title='Setup watchdog for debugging (depends on hardware)', default=False),
                ce.Boolean(key='BuildWithClang', title='Build with the Clang compiler', default=False),
                ce.Boolean(key='VerboseBuild', title='Verbose build output', default=False),
                ce.Boolean(key='DebugSymbols', title='Build with debug symbols (need lots of RAM, use of Clang advised)', default=False),
                ce.Boolean(key='EnableBulkOutputTest', title='Enable bulk output test commands (M942, M943)', default=False),
                ce.Boolean(key='EnableBasicTestModule', title='Enable basic test features (see BasicTestModule)', default=True),
                ce.Boolean(key='EnableStubCommandModule', title='Enable stub commands (see StubCommandModule)', default=True),
                ce.OneOf(key='NetworkTestModule', title='Enable network test module', choices=[
                    ce.Compound('Disabled', title='Disabled', attrs=[]),
                    ce.Compound('Enabled', title='Enabled', attrs=[
                        ce.Integer(key='BufferSize', title='Buffer size'),
                    ]),
                ]),
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
                ce.Boolean(key='StepLevel', title='Step level', default=True, false_title='Step at low, normally high', true_title='Step at high, normally low'),
                ce.Boolean(key='EnableLevel', title='Enable level', default=False, false_title='Enabled at low', true_title='Enabled at high'),
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
                input_mode_choice(board_context, key='InputMode', title='Input mode'),
            ])),
            ce.Array(key='analog_inputs', title='Analog inputs', copy_name_key='Name', processing_order=-7, elem=ce.Compound('analog_input', title='Analog input', title_key='Name', collapsable=True, attrs=[
                ce.String(key='Name', title='Name'),
                ce.OneOf(key='Driver', title='Driver', choices=[
                    ce.Compound('AdcAnalogInput', title='ADC pin', attrs=[
                        pin_choice(key='Pin', title='Pin'),
                    ]),
                    ce.Compound('Max31855AnalogInput', title='Thermocouple via MAX31855', attrs=[
                        pin_choice(key='SsPin', title='SS pin'),
                        spi_choice(key='SpiService', title='SPI driver'),
                    ]),
                ]),
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
        ])),
        ce.Reference(key='selected_config', title='Selected configuration (to compile)', ref_array={'base': 'id_editor.configurations', 'descend': []}, ref_id_key='name', ref_name_key='name'),
    ])
