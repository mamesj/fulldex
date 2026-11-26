# Writes usumfix.vert and usumfix.frag (Vulkan GLSL) from the OpenGL shader in usumfix.c, so the two builds share
# one shader:  python3 mkglsl.py usumfix.c OUT_DIR
import os, re, sys
src = open(sys.argv[1]).read()
out = sys.argv[2]
body = re.search(r'#define GLSL_BODY \\\n(.*?)\n\n', src, re.S).group(1)
lines = []
for l in body.split('\n'):
    l = l.rstrip()
    if l.endswith('\\'): l = l[:-1].rstrip()
    l = l.strip()
    if l: lines.append(l[1:-1].encode().decode('unicode_escape'))
glsl_body = ''.join(lines)
vs = re.search(r'static const char \*gl_vs =\n(.*?);\n', src, re.S).group(1)
vlines = [l.strip()[1:-1].encode().decode('unicode_escape') for l in vs.split('\n') if l.strip()]
vmain = ''.join(vlines)
vmain = vmain.split('void main(){', 1)[1]
vmain = vmain.replace('gl_VertexID', 'gl_VertexIndex')
vmain = vmain.replace(' gl_Position = vec4(px.x / win.x * 2.0 - 1.0, 1.0 - px.y / win.y * 2.0, 0.0, 1.0);\n',
    ' float y = win.z > 0.5 ? 1.0 - px.y / win.y * 2.0 : px.y / win.y * 2.0 - 1.0;\n gl_Position = vec4(px.x / win.x * 2.0 - 1.0, y, 0.0, 1.0);\n')
assert 'win.z > 0.5' in vmain   # (Vulkan's picture may be upside down: the window's flip is in win.z)
open(os.path.join(out, 'usumfix.vert'), 'w').write('#version 450\nlayout(binding = 2, std140) uniform UB { vec4 U[24]; };\n'
    'layout(location = 0) out vec2 v_px;\nvoid main(){' + vmain)
open(os.path.join(out, 'usumfix.frag'), 'w').write('#version 450\nlayout(binding = 2, std140) uniform UB { vec4 U[24]; };\n'
    'layout(binding = 0) uniform sampler2D u_bot;\nlayout(binding = 1) uniform sampler2D u_atlas;\n'
    'layout(location = 0) in vec2 v_px;\nlayout(location = 0) out vec4 o;\n' + glsl_body + 'void main(){ shade(v_px); }\n')
