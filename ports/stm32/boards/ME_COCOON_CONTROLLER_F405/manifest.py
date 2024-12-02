include("$(PORT_DIR)/boards/manifest.py")
freeze("$(PORT_DIR)/boards/ME_COCOON_CONTROLLER_F405/software")
freeze("$(PORT_DIR)/frozen")
require("ds18x20")
