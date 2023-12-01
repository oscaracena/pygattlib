
class BTBaseException(RuntimeError):
    """
    Base class for custom gattlib exceptions.
    """

class BTIOException(BTBaseException):
    """
    "Parameter, state and BT protocol level errors."
    """

class GATTException(BTBaseException):
    """
    GATT level errors.
    """


class AdapterNotFound(BTBaseException):
    def __init__(self, name):
        super().__init__(f"Adapter '{name}' not found!")


class DeviceNotFound(BTBaseException):
    def __init__(self, address, adapter):
        msg = f"Device with address '{address}' not found (using adapter '{adapter}')!"
        super().__init__(msg)
