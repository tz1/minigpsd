import mgsetuplib
import hildondesktop

class statusbar_applet(hildondesktop.StatusbarItem):
    def __init__(self):
        hildondesktop.StatusbarItem.__init__(self)
        self.mgsetuplib = mgsetuplib.MGSetup()
        self.add(self.mgsetuplib.setup(1))
        self.show_all()
        
def hd_plugin_get_objects():
    plugin = statusbar_applet()
    return [plugin]
