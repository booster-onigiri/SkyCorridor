"""Apply the exposure, atmosphere and outdoor leaf pass without reimporting FBX."""
from pathlib import Path
scripts=Path(__file__).resolve().parent
for name in ["finish_atmosphere.py","finish_city_light.py","finish_nature.py"]:
    source=scripts/name
    exec(compile(source.read_text(encoding="utf8"),str(source),"exec"),
         {"__name__":"endless_daylight_pass","__file__":str(source)})
