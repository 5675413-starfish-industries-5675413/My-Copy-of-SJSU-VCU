import sys
from pathlib import Path

HIL_ROOT = Path(__file__).resolve().parent
if str(HIL_ROOT) not in sys.path:
    sys.path.insert(0, str(HIL_ROOT))
