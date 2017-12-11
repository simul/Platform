#define GFXRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"DescriptorTable" \
"("\
	"CBV(b0, numDescriptors = 14)," \
	"SRV(t0, numDescriptors = 16)," \
	"UAV(u0, numDescriptors = 16)" \
")," \
"DescriptorTable" \
"(" \
	"Sampler(s0, numDescriptors = 16)" \
")" \