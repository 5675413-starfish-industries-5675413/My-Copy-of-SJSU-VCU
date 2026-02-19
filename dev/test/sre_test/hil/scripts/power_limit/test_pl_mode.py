import time

from sre_test.hil.core import (
    CANInterface,
    DBCLoader,
    HILParamInjector,
    MCM,
    PowerLimitController,
    PowerLimitMonitor,
)

with CANInterface() as can:
    dbc = DBCLoader("/Users/dangeb/Documents/College/SJSU/Spartan Racing/VCU/dev/test/sre_test/hil/dbcs/10.22.25_SRE_Main.dbc")
    injector = HILParamInjector(can)

    # Subsystem abstraction layers — designer doesn't care about transport
    pl = PowerLimitController(injector)
    pl_monitor = PowerLimitMonitor(dbc, can)

    # Set test conditions through abstraction layers

    pl.set_mode(1)

    # Flush all parameters to VCU
    pl.send_all()        # -> injector messages on 0x5FE

    # Monitor VCU response
    for _ in range(100):
        status = pl_monitor.recv(timeout=0.01)
        if status:
            print(f"PL Status: {status.status}, Mode: {status.mode}")
        time.sleep(0.01)

    # Disable power limiting
    # pl.set_mode(2)
    # pl.send_all()

print("Done.")