# Writes the SPIR-V files named as a C header of uint32_t arrays (usumfix.vert.spv -> usumfix_vert_spv):
#   python3 spv2h.py usumfix.vert.spv usumfix.frag.spv > vk_shaders.h
import os, struct, sys
out = ['/* made from usumfix.vert and usumfix.frag by glslangValidator -V (build.sh) */']
for fn in sys.argv[1:]:
    name = os.path.basename(fn).replace('.', '_')
    d = open(fn, 'rb').read()
    words = struct.unpack('<%dI' % (len(d) // 4), d)
    out.append('static const uint32_t %s[%d] = {' % (name, len(words)))
    for i in range(0, len(words), 8):
        out.append('    ' + ', '.join('0x%08x' % w for w in words[i:i + 8]) + ',')
    out.append('};')
print('\n'.join(out))
