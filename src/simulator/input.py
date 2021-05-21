class Input:
    def __init__(self, tabletInterface):
        self._was_dragging = False
        self.tabletInterface = tabletInterface

    def onDragStart(self, event):
        self._was_dragging = True
        self.tabletInterface.onDragStartCB(
            event.x // self.tabletInterface.scale,
            event.y // self.tabletInterface.scale)
         

    def onDragPoint(self, event):
        x = event.x // self.tabletInterface.scale
        y = event.y // self.tabletInterface.scale

        # If the pen left the screen then simulate the drag restarting.  This is how a real
        # touchscreen would respond.
        if x < 0 or y < 0 or x >= self.tabletInterface.screen_res[0] or y >= self.tabletInterface.screen_res[1]:
            # Constrain the values - a real touchscreen cannot detect presses
            # off the screen.  The last point then would be on-screen.
            if x < 0:
                x = 0
            if y < 0:
                y = 0
            if x >= self.tabletInterface.screen_res[0]:
                x = self.tabletInterface.screen_res[0] - 1
            if y >= self.tabletInterface.screen_res[1]:
                y = self.tabletInterface.screen_res[1] - 1

            if self._was_dragging:
                # Notify the software that the drag stopped
                self.tabletInterface.onDragStopCB(x, y)
                self._was_dragging = False

        else:
            # Don't call both onDragStart and onDragPoint for the same (x, y) unless
            # this function (_drag) is called twice.  This just checks and ensures the drag
            # restart is registered properly.
            if not self._was_dragging:
                self.tabletInterface.onDragPointCB(x, y)
                self._was_dragging = True
            else:
                self.tabletInterface.onDragPointCB(x, y)
                

    def onDragStop(self, event):
        # If the pen is pulled offscreen, _drag will send an onDragStop
        # event.  If the user stops clicking off-screen then this function
        # is called, which would make onDragStop called again.  Instead
        # we'll check if it was already called first.
        if self._was_dragging:
            self.tabletInterface.onDragStopCB(
                event.x // self.tabletInterface.scale,
                event.y // self.tabletInterface.scale)
            
