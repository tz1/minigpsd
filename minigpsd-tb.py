import mgsetuplib
import hildondesktop

class taskbar_applet(hildondesktop.TaskNavigatorItem):
    def __init__(self):
        hildondesktop.TaskNavigatorItem.__init__(self)
        self.mgsetuplib = mgsetuplib.MGSetup()
        self.add(self.mgsetuplib.setup(2))
        self.add(self.mgsetuplib.menu)
        self.show_all()
        
def hd_plugin_get_objects():
    plugin = taskbar_applet()
    return [plugin]
