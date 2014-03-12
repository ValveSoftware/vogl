!!ARBfp1.0

# fog.fp
#
# per-pixel fog

ATTRIB iPrC = fragment.color.primary;  # input primary color
ATTRIB iFrP = fragment.position;       # input fragment position

OUTPUT oPrC = result.color;            # output color

PARAM density = program.local[0];      # fog density
PARAM fogColor = {0.5, 0.8, 0.5, 1.0}; # fog color
PARAM e = {2.71828, 0, 0, 0};

TEMP fogFactor;

# fogFactor = clamp(e^(-(d*Zw)^2))
MUL fogFactor.x, iFrP.z, density.x;
MUL fogFactor.x, fogFactor.x, fogFactor.x;
POW fogFactor.x, e.x, -fogFactor.x;
MAX fogFactor.x, fogFactor.x, 0.0;     # clamp to [0,1]
MIN fogFactor.x, fogFactor.x, 1.0;

LRP oPrC, fogFactor.x, iPrC, fogColor; # blend lit and fog colors

END
