#Name: DeTool
#Info: Postprocessor for APrinter firmware
#Depend: GCode
#Type: postprocess
#Param: sdcard_param(float:0) Compress for SD card printing
#Param: toolTravelSpeed(float:0) Tool change travel speed (mm/s)
#Param: t0extruder(string:0) T0 PhysicalExtruder (empty=none)
#Param: t1extruder(string:1) T1 PhysicalExtruder (empty=none)
#Param: t2extruder(string:2) T2 PhysicalExtruder (empty=none)
#Param: axis0(string:E) PhysicalExtruder0 axis
#Param: offset0X(float:0) X offset (mm)
#Param: offset0Y(float:0) Y offset (mm)
#Param: offset0Z(float:0) Z offset (mm)
#Param: fan0(string:M106) Fan command
#Param: fan_multiplier0(float:1.0) Fan speed multiplier
#Param: axis1(string:U) PhysicalExtruder1 axis
#Param: offset1X(float:0) X offset (mm)
#Param: offset1Y(float:0) Y offset (mm)
#Param: offset1Z(float:0) Z offset (mm)
#Param: fan1(string:M406) Fan command
#Param: fan_multiplier1(float:1.0) Fan speed multiplier
#Param: axis2(string:V) PhysicalExtruder2 axis
#Param: offset2X(float:0) X offset (mm)
#Param: offset2Y(float:0) Y offset (mm)
#Param: offset2Z(float:0) Z offset (mm)
#Param: fan2(string:M506) Fan command
#Param: fan_multiplier2(float:1.0) Fan speed multiplier

"""
 * Copyright (c) 2013 Ambroz Bizjak
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
"""

__copyright__ = "Copyright (C) 2013 Ambroz Bizjak - Released under the BSD 2-clause license"

def replace_multi(subject, match, replace):
    assert sum([len(m) == 0 for m in match]) == 0
    assert len(replace) == len(match)
    pos = 0
    result = []
    while True:
        nearest_pos = len(subject)
        for i in range(len(match)):
            match_pos = subject.find(match[i], pos)
            if match_pos >= 0 and match_pos < nearest_pos:
                nearest_pos = match_pos
                nearest_match = i
        result.append(subject[pos:nearest_pos])
        if nearest_pos == len(subject):
            break
        result.append(replace[nearest_match])
        pos = nearest_pos + len(match[nearest_match])
    return ''.join(result)

physicalExtruders = {}
tools = {}

if 'filename' in locals():
    physicalExtruders[0] = {'name':axis0, 'offsets':{'X':offset0X, 'Y':offset0Y, 'Z':offset0Z}, 'fan':fan0, 'fan_multiplier':fan_multiplier0}
    physicalExtruders[1] = {'name':axis1, 'offsets':{'X':offset1X, 'Y':offset1Y, 'Z':offset1Z}, 'fan':fan1, 'fan_multiplier':fan_multiplier1}
    physicalExtruders[2] = {'name':axis2, 'offsets':{'X':offset2X, 'Y':offset2Y, 'Z':offset2Z}, 'fan':fan2, 'fan_multiplier':fan_multiplier2}
    
    if t0extruder:
        tools[0] = physicalExtruders[int(t0extruder)]
    if t1extruder:
        tools[1] = physicalExtruders[int(t1extruder)]
    if t2extruder:
        tools[2] = physicalExtruders[int(t2extruder)]
    
    inputFileName = filename
    outputFileName = filename
    sdcard = (sdcard_param != 0.0)
    
else:
    import argparse
    
    parser = argparse.ArgumentParser(description='GCode post-processor for APrinter firmware.')
    parser.add_argument('--input', dest='input', metavar='InputFile', required=True)
    parser.add_argument('--output', dest='output', metavar='OutputFile', required=True)
    parser.add_argument('--tool-travel-speed', dest='tool_travel_speed', metavar='Speedmm/s', required=True)
    parser.add_argument('--physical', dest='physical', action='append', nargs=4, metavar=('AxisName', 'OffsetX', 'OffsetY', 'OffsetZ'), required=True)
    parser.add_argument('--tool', dest='tool', action='append', nargs=2, metavar=('ToolIndex', 'PhysicalIndexFrom0'), required=True)
    parser.add_argument('--fan', dest='fan', action='append', nargs=3, metavar=('FanSpeedCmd', 'PhysicalIndexFrom0', 'SpeedMultiplier'))
    parser.add_argument('--sdcard', dest='sdcard', action='store_true')
    
    args = parser.parse_args()
    inputFileName = args.input
    outputFileName = args.output
    toolTravelSpeed = float(args.tool_travel_speed)
    for p in args.physical:
        physicalExtruders[len(physicalExtruders)] = {'name':p[0], 'offsets':{'X':float(p[1]), 'Y':float(p[2]), 'Z':float(p[3])}, 'fan':''}
    for t in args.tool:
        if not t[0].isdigit():
            raise Exception('Tool index is invalid')
        if not (t[1].isdigit() and int(t[1]) in physicalExtruders):
            raise Exception('Tool physical index is invalid')
        tools[int(t[0])] = physicalExtruders[int(t[1])]
    if args.fan:
        for f in args.fan:
            if not (f[1].isdigit() and int(f[1]) in physicalExtruders):
                raise Exception('Fan physical index is invalid')
            physicalExtruders[int(f[1])]['fan'] = f[0]
            physicalExtruders[int(f[1])]['fan_multiplier'] = float(f[2])
    sdcard = bool(args.sdcard)

with open(inputFileName, "r") as f:
    lines = f.readlines()

currentTool = min(tools.keys())
currentRelative = False
currentPhysPos = {'X':0.0, 'Y':0.0, 'Z':0.0}
currentReqPos = {'X':0.0, 'Y':0.0, 'Z':0.0, 'E':0.0}
currentKnown = {'X':False, 'Y':False, 'Z':False}
currentPending = {'X':False, 'Y':False, 'Z':False}
currentF = 999999.0
currentFanSpeed = 0.0
currentIgnore = False

subst_match = ['{T%sAxis}' % (i) for i in tools] + ['?T%sAxis?' % (i) for i in tools]
subst_replace = [tools[i]['name'] for i in tools] + [tools[i]['name'] for i in tools]

with open(outputFileName, "w") as f:
    if not sdcard:
        f.write(';DeTool init\n')
    f.write('G90\n')
    for tool in tools:
        f.write('G92 %s%.5f\n' % (tools[tool]['name'], currentReqPos['E']))
    f.write('G0 F%.1f\n' % (currentF))
    if tools[currentTool]['fan']:
        f.write('%s S%.2f\n' % (tools[currentTool]['fan'], currentFanSpeed * tools[currentTool]['fan_multiplier']))
    if not sdcard:
        f.write(';DeTool init end\n')
    
    for line in lines:
        line = line.strip()
        line = replace_multi(line, subst_match, subst_replace)
        commentPos = line.find(';')
        if commentPos < 0:
            commentPos = len(line)
        dataLine = line[:commentPos]
        commentLine = line[commentPos:]
        oldIgnore = currentIgnore
        if commentLine.find('DeToolIgnoreSection') >= 0:
            currentIgnore = True
        elif commentLine.find('DeToolEndIgnoreSection') >= 0:
            currentIgnore = False
        comps = dataLine.split()
        newLine = (dataLine if sdcard else line) + '\n'
        if len(comps) == 0 or oldIgnore or commentLine.find('DeToolKeep') >= 0:
            if not sdcard or len(comps) != 0:
                f.write(newLine)
            continue
        
        if comps[0].startswith('T'):
            newLine = ''
            toolStr = comps[0][1:]
            if not (toolStr.isdigit() and int(toolStr) in tools):
                raise Exception('Invalid tool in T command')
            newTool = int(toolStr)
            if newTool != currentTool:
                toolName = tools[newTool]['name']
                if not sdcard:
                    newLine += ';DeTool switch to tool %s (%s)\n' % (newTool, toolName)
                newLine += 'G92 %s%.5f\n' % (toolName, currentReqPos['E'])
                for axisName in currentPhysPos:
                    if not currentKnown[axisName]:
                        raise Exception('Got tool change while position is unknown')
                    currentPending[axisName] = True
                if tools[currentTool]['fan']:
                    newLine += '%s S0\n' % (tools[currentTool]['fan'])
                if tools[newTool]['fan']:
                    newLine += '%s S%.2f\n' % (tools[newTool]['fan'], currentFanSpeed * tools[newTool]['fan_multiplier'])
                if not sdcard:
                    newLine += ';DeTool switch end\n'
                currentTool = newTool
            
        elif comps[0] == 'G28':
            homeAxes = []
            for i in range(1, len(comps)):
                axisName = comps[i][0]
                if not axisName in currentPhysPos:
                    raise Exception('Got G28 with unknown axis')
                homeAxes.append(axisName)
            if len(homeAxes) == 0:
                homeAxes = currentPhysPos.keys()
            for axisName in homeAxes:
                currentKnown[axisName] = False
                currentPending[axisName] = False
            
        elif comps[0] == 'G90':
            currentRelative = False
            newLine = ''
            if not sdcard:
                newLine = ';DeTool absolute\n'
            
        elif comps[0] == 'G91':
            currentRelative = True
            newLine = ''
            if not sdcard:
                newLine = ';DeTool relative\n'
            
        elif comps[0] == 'G92':
            newComps = [comps[0]]
            for i in range(1, len(comps)):
                comp = comps[i]
                if len(comp) == 0 or not comp[0] in currentReqPos:
                    raise Exception('Got G92 with unknown axis')
                axisName = comp[0]
                value = float(comp[1:])
                if axisName == 'E':
                    comp = '%s%.5f' % (tools[currentTool]['name'], value)
                else:
                    if currentKnown[axisName]:
                        currentPhysPos[axisName] += value - currentReqPos[axisName]
                    else:
                        currentPhysPos[axisName] = value + tools[currentTool]['offsets'][axisName]
                        currentKnown[axisName] = True
                    comp = '%s%.5f' % (axisName, currentPhysPos[axisName])
                currentReqPos[axisName] = value
                newComps.append(comp)
            newLine = '%s\n' % (' '.join(newComps))
        
        elif comps[0] == 'M106' or comps[0] == 'M107':
            for i in range(1, len(comps)):
                comp = comps[i]
                if comps[0] == 'M106' and len(comp) > 0 and comp[0] == 'S':
                    currentFanSpeed = float(comp[1:])
                else:
                    raise Exception('Got unknown parameter in M106 or M107')
            if comps[0] == 'M107':
                currentFanSpeed = 0.0
            newLine = ''
            if tools[currentTool]['fan']:
                newLine = '%s S%.2f\n' % (tools[currentTool]['fan'], currentFanSpeed * tools[currentTool]['fan_multiplier'])
        
        elif comps[0] == 'G0' or comps[0] == 'G1':
            newLine = ''
            if comps[0] == 'G1':
                pendingAxes = []
                for axisName in currentPhysPos:
                    if currentPending[axisName]:
                        assert currentKnown[axisName]
                        pendingAxes.append(axisName)
                        currentPhysPos[axisName] = currentReqPos[axisName] + tools[currentTool]['offsets'][axisName]
                        currentPending[axisName] = False
                if len(pendingAxes) > 0:
                    if not sdcard:
                        newLine += ';DeTool travel after tool change\n'
                    newLine += \
                        'G0 %s F%.1f\n' % (' '.join(['%s%.5f' % (axisName, currentPhysPos[axisName]) for axisName in pendingAxes]), toolTravelSpeed * 60.0) + \
                        'G0 F%.1f\n' % (currentF)
                    if not sdcard:
                        newLine += ';DeTool travel after tool change end\n'
                newF = currentF
            elif sum(currentPending.values()) > 0:
                if not sdcard:
                    newLine += ';DeTool merging tool change with G0\n'
            newReqPos = currentReqPos.copy()
            seenAxes = []
            for i in range(1, len(comps)):
                comp = comps[i]
                if len(comp) > 0 and comp[0] == 'F':
                    newF = float(comp[1:])
                elif len(comp) > 0 and comp[0] in currentReqPos:
                    if currentRelative:
                        if comp[0] != 'E' and not currentKnown[comp[0]]:
                            raise Exception('Got relative move with axis whose position is unknown')
                        newReqPos[comp[0]] += float(comp[1:])
                    else:
                        newReqPos[comp[0]] = float(comp[1:])
                    seenAxes.append(comp[0])
                else:
                    raise Exception('Unknown axis in G0 or G1')
            newLine += comps[0]
            if newF != currentF:
                newLine += ' F%.1f' % (newF)
            for axisName in currentReqPos:
                if axisName in seenAxes or (axisName != 'E' and currentPending[axisName]):
                    axisCurrentPhysPos = currentPhysPos[axisName] if axisName != 'E' else currentReqPos[axisName]
                    axisReqPhysPos = newReqPos[axisName]
                    if axisName != 'E':
                        axisReqPhysPos += tools[currentTool]['offsets'][axisName]
                    if (axisName != 'E' and not currentKnown[axisName]) or axisCurrentPhysPos != axisReqPhysPos:
                        realAxisName = axisName if axisName != 'E' else tools[currentTool]['name']
                        newLine += ' %s%.5f' % (realAxisName, axisReqPhysPos)
                        if axisName != 'E':
                            currentPhysPos[axisName] = axisReqPhysPos
                    if axisName != 'E':
                        currentPending[axisName] = False
                        currentKnown[axisName] = True
            currentF = newF
            currentReqPos = newReqPos
            newLine += '\n'
        
        f.write(newLine)
        
    if not sdcard:
        f.write(';DeTool end\n')
    if sdcard:
        f.write('EOF\n')
