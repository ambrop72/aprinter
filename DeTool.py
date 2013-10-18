#Name: DeTool
#Info: Convert tool commands into independent axes
#Depend: GCode
#Type: postprocess
#Param: t0extruder(string:0) T0 PhysicalExtruder (empty=none)
#Param: t1extruder(string:1) T1 PhysicalExtruder (empty=none)
#Param: t2extruder(string:2) T2 PhysicalExtruder (empty=none)
#Param: axis0(string:E) PhysicalExtruder0 axis
#Param: offset0X(float:0) X offset (mm)
#Param: offset0Y(float:0) Y offset (mm)
#Param: offset0Z(float:0) Z offset (mm)
#Param: axis1(string:U) PhysicalExtruder1 axis
#Param: offset1X(float:0) X offset (mm)
#Param: offset1Y(float:0) Y offset (mm)
#Param: offset1Z(float:0) Z offset (mm)
#Param: axis2(string:V) PhysicalExtruder2 axis
#Param: offset2X(float:0) X offset (mm)
#Param: offset2Y(float:0) Y offset (mm)
#Param: offset2Z(float:0) Z offset (mm)

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

with open(filename, "r") as f:
    lines = f.readlines()

physicalExtruders = {}
physicalExtruders[0] = {'name':axis0, 'offsets':{'X':offset0X, 'Y':offset0Y, 'Z':offset0Z}}
physicalExtruders[1] = {'name':axis1, 'offsets':{'X':offset1X, 'Y':offset1Y, 'Z':offset1Z}}
physicalExtruders[2] = {'name':axis2, 'offsets':{'X':offset2X, 'Y':offset2Y, 'Z':offset2Z}}

tools = {}
if t0extruder:
    tools[0] = physicalExtruders[int(t0extruder)]
if t1extruder:
    tools[1] = physicalExtruders[int(t1extruder)]
if t2extruder:
    tools[2] = physicalExtruders[int(t2extruder)]

currentTool = 0
currentRelative = False
currentPos = {'X':0.0, 'Y':0.0, 'Z':0.0, 'E':0.0}
currentF = 0.0

with open(filename, "w") as f:
    f.write(';DeTool init\n')
    f.write('G90\n')
    f.write('G0 F0\n')
    for tool in tools:
        f.write('G92 %s0\n' % (tools[tool]['name']))
    for line in lines:
        newLine = line
        comps = line.split()
        if len(comps) > 0 and comps[0].startswith('T'):
            currentTool = int(comps[0][1:])
            toolName = tools[currentTool]['name']
            newLine = \
                ';DeTool switch to tool %s (%s), Pos=%s, F=%f, relative=%s\n' % (currentTool, toolName, repr(currentPos), currentF, currentRelative) + \
                'G92 %s%.5f\n' % (toolName, currentPos['E'])
            if not 'S' in comps:
                newLine = newLine + \
                'G0 %s F999999\n' % ' '.join(['%s%.5f' % (coord, currentPos[coord] + tools[currentTool]['offsets'][coord]) for coord in currentPos if coord != 'E']) + \
                'G0 F%f\n' % (currentF)
        elif len(comps) > 0 and comps[0].startswith('G90'):
            currentRelative = False
        elif len(comps) > 0 and comps[0].startswith('G91'):
            currentRelative = True
        elif len(comps) > 0 and comps[0].startswith('G92'):
            newComps = [comps[0]]
            for i in range(1, len(comps)):
                comp = comps[i]
                if len(comp) > 0 and comp[0] in currentPos:
                    value = float(comp[1:])
                    currentPos[comp[0]] = value
                    if comp[0] == 'E':
                        comp = '%s%s' % (tools[currentTool]['name'], comp[1:])
                    elif comp[0] in tools[currentTool]['offsets']:
                        value = float(comp[1:]) + tools[currentTool]['offsets'][comp[0]]
                        comp = '%s%.5f' % (comp[0], value)
                newComps.append(comp)
            newLine = '%s\n' % (' '.join(newComps))
        elif len(comps) > 0 and (comps[0].startswith('G0') or comps[0].startswith('G1')):
            newComps = [comps[0]]
            for i in range(1, len(comps)):
                comp = comps[i]
                if len(comp) > 0 and comp[0] in currentPos:
                    value = float(comp[1:])
                    if currentRelative:
                        currentPos[comp[0]] += value
                    else:
                        currentPos[comp[0]] = value
                    if comp[0] == 'E':
                        comp = '%s%s' % (tools[currentTool]['name'], comp[1:])
                    elif comp[0] in tools[currentTool]['offsets']:
                        value = float(comp[1:]) + tools[currentTool]['offsets'][comp[0]]
                        comp = '%s%.5f' % (comp[0], value)
                elif len(comp) > 0 and comp[0] == 'F':
                    currentF = float(comp[1:])
                newComps.append(comp)
            newLine = '%s\n' % (' '.join(newComps))
        f.write(newLine)
