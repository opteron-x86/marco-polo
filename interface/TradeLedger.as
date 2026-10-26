// Native Skyrim Scaleform menu. Fonts are resolved by the game's active fontconfig.
class TradeLedger {
    static var instance:TradeLedger;
    var root:MovieClip;
    var panel:MovieClip;
    var content:MovieClip;
    var tooltip:MovieClip;
    var data:Object;
    var tab:Number = 0;
    var hold:Number = 0;
    var page:Number = 0;
    var pages:Number = 1;
    var perPage:Number = 8;
    var choosing:Boolean = false;
    var pinnedOnly:Boolean = false;
    var dragging:Boolean = false;
    var resizing:Boolean = false;
    var resizeX:Number;
    var resizeY:Number;
    var resizeWidth:Number;
    var resizeHeight:Number;
    var resizeFirst:Number;
    var prefs:Object;
    var w:Number = 460;
    var h:Number = 430;
    var gold:Number = 0xD4BD8A;
    var white:Number = 0xE4DFD3;
    var muted:Number = 0xA9A99E;

    static function main(mc:MovieClip):Void { instance = new TradeLedger(mc); }
    function TradeLedger(mc:MovieClip) {
        root = mc;
        root.stop();
        Stage.scaleMode = "noScale";
        Stage.align = "TL";
        var self:TradeLedger = this;
        root.SetData = function(d:Object):Void { self.setData(d); };
        root.HandleKey = function(key:String):Void { self.command(key); };
        root.SetPlatform = function(platform:Number, ps3:Boolean):Void {};
        root.InitExtensions = function():Void {};
        Stage.addListener(this);
        Mouse.addListener(this);
        Key.addListener(this);
    }
    function native(action:String, value:Object):Void { root.STEMNative(action, value); }
    function clip(parent:MovieClip, name:String, x:Number, y:Number):MovieClip {
        var mc:MovieClip = parent.createEmptyMovieClip(name, parent.getNextHighestDepth());
        mc._x = x; mc._y = y;
        return mc;
    }
    function rect(mc:MovieClip, x:Number, y:Number, width:Number, height:Number, color:Number, alpha:Number):Void {
        mc.beginFill(color, alpha);
        mc.moveTo(x, y); mc.lineTo(x + width, y); mc.lineTo(x + width, y + height);
        mc.lineTo(x, y + height); mc.lineTo(x, y); mc.endFill();
    }
    function line(mc:MovieClip, x:Number, y:Number, x2:Number, y2:Number, color:Number):Void {
        mc.lineStyle(1, color, 70); mc.moveTo(x, y); mc.lineTo(x2, y2); mc.lineStyle();
    }
    function label(mc:MovieClip, name:String, text:String, x:Number, y:Number, width:Number,
                   size:Number, color:Number, align:String):TextField {
        mc.createTextField(name, mc.getNextHighestDepth(), x, y, width, size + 12);
        var field:TextField = mc[name];
        field.selectable = false; field.embedFonts = true;
        var format:TextFormat = new TextFormat(size >= 23 ? "$EverywhereMediumFont" : "$EverywhereFont", size, color);
        format.align = align; format.leftMargin = 0; format.rightMargin = 0;
        field.setNewTextFormat(format);
        field.text = text;
        // CommonLib's loader supplies the game's FontLib and font mappings.
        return field;
    }
    function button(parent:MovieClip, name:String, text:String, x:Number, y:Number, width:Number,
                    height:Number, action:String, value:Object, selected:Boolean, help:String):MovieClip {
        var b:MovieClip = clip(parent, name, x, y);
        rect(b, 0, 0, width, height, selected ? 0x756344 : 0x30322F, selected ? 75 : 55);
        if (text != "") label(b, "caption", text, 2, (height - 26) / 2, width - 4, 18, selected ? gold : white, "center");
        var self:TradeLedger = this;
        b.useHandCursor = true;
        b.onRollOver = function():Void { this._alpha = 78; if (help != "") self.showTip(help); };
        b.onRollOut = function():Void { this._alpha = 100; self.hideTip(); };
        b.onReleaseOutside = b.onRollOut;
        b.onRelease = function():Void { self.hideTip(); self.action(action, value); };
        return b;
    }
    function setData(d:Object):Void {
        data = d; prefs = d.preferences;
        hold = 0;
        for (var i:Number = 0; i < data.markets.length; ++i)
            if (data.markets[i].id == data.current) hold = i;
        panel = clip(root, "ledger", 0, 0);
        place();
        native("ready", {font: "$EverywhereFont", titleWidth: panel.title.textWidth,
                         stageWidth: Stage.width, stageHeight: Stage.height});
    }
    function factor():Number { return Math.max(.65, Math.min(3, Stage.height / 900)); }
    function place():Void {
        if (panel == undefined || Stage.width < 32 || Stage.height < 32) return;
        // Resolution determines the UI scale; dragging changes only the window bounds.
        var s:Number = Math.min(factor(), (Stage.width - 16) / 420, (Stage.height - 16) / 360);
        panel._xscale = panel._yscale = s * 100;
        w = Math.max(420, Math.min(1600, prefs.width, (Stage.width - 16) / s));
        h = Math.max(360, Math.min(1200, prefs.height, (Stage.height - 16) / s));
        var maxX:Number = Math.max(8, Stage.width - w * s - 8);
        var maxY:Number = Math.max(8, Stage.height - h * s - 8);
        panel._x = 8 + (maxX - 8) * prefs.x;
        panel._y = 8 + (maxY - 8) * prefs.y;
        draw();
    }
    function onResize():Void {
        panel.stopDrag(); delete root.onEnterFrame;
        dragging = resizing = false;
        place();
    }
    function remember():Void {
        var s:Number = panel._xscale / 100;
        var dx:Number = Stage.width - w * s - 16;
        var dy:Number = Stage.height - h * s - 16;
        prefs.x = dx > 0 ? Math.max(0, Math.min(1, (panel._x - 8) / dx)) : 0;
        prefs.y = dy > 0 ? Math.max(0, Math.min(1, (panel._y - 8) / dy)) : 0;
        prefs.width = w; prefs.height = h;
        native("preferences", prefs);
    }
    function resizeToMouse():Void {
        var s:Number = panel._xscale / 100;
        var width:Number = Math.max(420, Math.min(1600,
            Math.round(resizeWidth + (root._xmouse - resizeX) / s),
            Math.floor((Stage.width - 8 - panel._x) / s)));
        var height:Number = Math.max(360, Math.min(1200,
            Math.round(resizeHeight + (root._ymouse - resizeY) / s),
            Math.floor((Stage.height - 8 - panel._y) / s)));
        if (width == w && height == h) return;
        w = width; h = height;
        prefs.width = w; prefs.height = h;
        draw();
    }
    function draw():Void {
        hideTip();
        panel.clear();
        // Recreate children, but keep the root instance and its font library.
        for (var name:String in panel) {
            if (typeof(panel[name]) == "movieclip") panel[name].removeMovieClip();
            else if (panel[name] instanceof TextField) panel[name].removeTextField();
        }
        rect(panel, 0, 0, w, h, 0x151C1A, 94);
        line(panel, 0, 0, w, 0, gold); line(panel, 0, h, w, h, gold);
        line(panel, 0, 0, 0, h, gold); line(panel, w, 0, w, h, gold);
        var handle:MovieClip = clip(panel, "handle", 1, 1);
        rect(handle, 0, 0, w - 92, 42, 0x222924, 80);
        var self:TradeLedger = this;
        handle.onPress = function():Void {
            if (self.prefs.locked) return;
            self.hideTip(); self.dragging = true;
            var s:Number = self.panel._xscale / 100;
            self.panel.startDrag(false, 8, 8, Math.max(8, Stage.width - self.w * s - 8), Math.max(8, Stage.height - self.h * s - 8));
        };
        label(panel, "title", "Trade Ledger", 14, 6, w - 112, 24, gold, "left");
        var lock:MovieClip = button(panel, "lock", "", w - 86, 8, 32, 28, "lock", null, prefs.locked,
                                   prefs.locked ? "Unlock position" : "Lock position");
        rect(lock, 10, 12, 12, 10, gold, 100);
        lock.lineStyle(2, gold, 100); lock.moveTo(12, 13); lock.lineTo(12, 7);
        lock.lineTo(20, 7); lock.lineTo(20, prefs.locked ? 13 : 9); lock.lineStyle();
        var close:MovieClip = button(panel, "close", "", w - 48, 8, 32, 28, "close", null, false, "Close");
        line(close, 11, 9, 21, 19, white); line(close, 11, 19, 21, 9, white);
        var tabs:Array = ["Markets", "Routes", "Rumors"];
        var tabWidth:Number = (w - 32) / 3;
        for (var t:Number = 0; t < tabs.length; ++t)
            button(panel, "tab" + t, tabs[t], 12 + t * (tabWidth + 4), 47, tabWidth, 31, "tab", t, tab == t, "");
        content = clip(panel, "content", 0, 0);
        if (tab == 0) markets();
        else if (tab == 1) routes();
        else rumors();
        line(panel, 12, h - 40, w - 12, h - 40, gold);
        label(panel, "date", data.date, 13, h - 32, w - 197, 14, muted, "left");
        button(panel, "back", "<", w - 180, h - 34, 28, 25, "page", -1, false, "Previous page");
        label(panel, "page", String(page + 1) + " / " + String(pages), w - 148, h - 32, 77, 15, white, "center");
        button(panel, "next", ">", w - 68, h - 34, 28, 25, "page", 1, false, "Next page");
        var grip:MovieClip = button(panel, "grip", "", w - 28, h - 28, 26, 26, "noop", null, false,
                                   prefs.locked ? "Position locked" : "Drag to resize");
        line(grip, 9, 22, 22, 9, gold); line(grip, 16, 22, 22, 16, gold);
        grip.onPress = function():Void {
            if (self.prefs.locked) return;
            self.hideTip(); self.resizing = true;
            self.choosing = false;
            self.resizeX = self.root._xmouse; self.resizeY = self.root._ymouse;
            self.resizeWidth = self.w; self.resizeHeight = self.h;
            self.resizeFirst = self.page * self.perPage;
            self.root.onEnterFrame = function():Void { self.resizeToMouse(); };
        };
        if (choosing && tab == 0) chooser();
    }
    function paginate(length:Number, count:Number):Void {
        count = Math.max(1, count);
        if (count != perPage) page = Math.floor((resizing ? resizeFirst : page * perPage) / count);
        perPage = count;
        pages = Math.max(1, Math.ceil(length / count));
        page = Math.max(0, Math.min(page, pages - 1));
    }
    function tone(n:Number):Number { return n == -1 ? 0xA5C49E : n == 1 ? 0xE1B875 : n == 0 ? white : muted; }
    function hover(mc:MovieClip, text:String):Void {
        var self:TradeLedger = this;
        mc.useHandCursor = false;
        mc.onRollOver = function():Void { if (!self.choosing) self.showTip(text); };
        mc.onRollOut = function():Void { self.hideTip(); };
    }
    function markets():Void {
        var count:Number = Math.floor((h - 40 - 149) / 29);
        if (data.markets.length == 0) { empty("No market reports"); paginate(0, count); return; }
        var market:Object = data.markets[hold];
        var inner:Number = w - 24;
        var nameWidth:Number = inner - 226;
        button(content, "holdBack", "<", 12, 88, 30, 30, "holdStep", -1, false, "Previous hold");
        button(content, "hold", market.name, 46, 88, w - 92, 30, "chooser", null, false, "Choose a hold");
        button(content, "holdNext", ">", w - 42, 88, 30, 30, "holdStep", 1, false, "Next hold");
        label(content, "goods", "Goods", 14, 124, nameWidth, 14, muted, "left");
        label(content, "prices", "Prices", nameWidth + 20, 124, 136, 14, muted, "left");
        label(content, "age", "Report", inner - 66, 124, 74, 14, muted, "right");
        paginate(market.rows.length, count);
        for (var i:Number = 0; i < perPage && page * perPage + i < market.rows.length; ++i) {
            var r:Object = market.rows[page * perPage + i];
            var row:MovieClip = clip(content, "row" + i, 12, 149 + i * 29);
            rect(row, 0, 0, inner, 28, i % 2 == 0 ? 0x4B5146 : 0x222A24, i % 2 == 0 ? 20 : 10);
            label(row, "name", r.name, 2, 1, nameWidth, 17, white, "left");
            label(row, "price", r.price, nameWidth + 8, 1, 136, 17, tone(r.tone), "left");
            label(row, "age", r.age, inner - 78, 3, 74, 14, muted, "right");
            hover(row, r.detail);
        }
    }
    function chooser():Void {
        var c:MovieClip = clip(panel, "chooser", 12, 121);
        var rows:Number = Math.ceil(data.markets.length / 2);
        // A separate backing target blocks gaps without swallowing the hold buttons.
        var backing:MovieClip = clip(c, "backing", 0, 0);
        rect(backing, 0, 0, w - 24, rows * 36 + 12, 0x131A17, 100);
        var self:TradeLedger = this;
        backing.useHandCursor = false;
        backing.onRollOver = function():Void { self.hideTip(); };
        backing.onPress = function():Void {};
        var column:Number = (w - 40) / 2;
        for (var i:Number = 0; i < data.markets.length; ++i)
            button(c, "m" + i, data.markets[i].name, 6 + (i % 2) * (column + 4), 6 + Math.floor(i / 2) * 36,
                   column, 32, "hold", i, hold == i, "");
    }
    function routeList():Array {
        var result:Array = [];
        // Bookmarks rise to the top without changing the underlying remembered reports.
        for (var pass:Number = 0; pass < 2; ++pass)
            for (var i:Number = 0; i < data.routes.length; ++i) {
                var r:Object = data.routes[i];
                if (r.pinned == (pass == 0) && (!pinnedOnly || r.pinned)) result.push(r);
            }
        return result;
    }
    function routes():Void {
        var inner:Number = w - 24;
        var half:Number = inner / 2;
        label(content, "hint", "Possible routes", 14, 88, w - 210, 16, muted, "left");
        button(content, "pinned", pinnedOnly ? "Pinned" : "All routes", w - 126, 88, 114, 27,
               "pinnedOnly", null, pinnedOnly, "Show bookmarked routes");
        var list:Array = routeList();
        paginate(list.length, Math.floor((h - 40 - 123) / 87));
        if (list.length == 0) empty(pinnedOnly ? "No pinned routes" : "No routes in your reports yet");
        for (var i:Number = 0; i < perPage && page * perPage + i < list.length; ++i) {
            var r:Object = list[page * perPage + i];
            var row:MovieClip = clip(content, "r" + i, 12, 123 + i * 87);
            rect(row, 0, 0, inner, 81, 0x4B5146, 20);
            label(row, "good", r.name, 8, 2, inner - 57, 19, gold, "left");
            var pin:MovieClip = button(row, "pin", "", inner - 35, 4, 27, 24, "pin", r.key, r.pinned,
                                       r.pinned ? "Unpin route" : "Pin route");
            pin.lineStyle(1, gold, 100); if (r.pinned) pin.beginFill(gold, 100);
            pin.moveTo(9, 5); pin.lineTo(18, 5); pin.lineTo(18, 19); pin.lineTo(13.5, 15);
            pin.lineTo(9, 19); pin.lineTo(9, 5); if (r.pinned) pin.endFill(); pin.lineStyle();
            label(row, "from", r.buy.market, 8, 29, half - 24, 17, white, "left");
            label(row, "arrow", ">", half - 13, 29, 24, 18, gold, "center");
            label(row, "to", r.sell.market, half + 17, 29, half - 25, 17, white, "left");
            label(row, "buy", r.buy.price + "  /  " + r.buy.age, 8, 54, half - 16, 14, tone(r.buy.tone), "left");
            label(row, "sell", r.sell.price + "  /  " + r.sell.age, half + 17, 54, half - 20, 14, tone(r.sell.tone), "left");
            // Separate hover target avoids swallowing the bookmark button.
            var area:MovieClip = clip(row, "info", 0, 28);
            rect(area, 0, 0, inner, 53, 0, 0);
            hover(area, r.detail);
        }
    }
    function rumors():Void {
        var inner:Number = w - 24;
        label(content, "hint", "Word from the inns", 14, 88, w - 34, 16, muted, "left");
        paginate(data.rumors.length, Math.floor((h - 40 - 122) / 65));
        if (data.rumors.length == 0) empty("No trade rumors heard yet");
        for (var i:Number = 0; i < perPage && page * perPage + i < data.rumors.length; ++i) {
            var r:Object = data.rumors[page * perPage + i];
            var row:MovieClip = clip(content, "r" + i, 12, 122 + i * 65);
            rect(row, 0, 0, inner, 61, 0x4B5146, 20);
            label(row, "market", r.market, 8, 0, inner - 152, 18, gold, "left");
            label(row, "age", r.age, inner - 136, 1, 128, 14, muted, "right");
            label(row, "good", r.name, 8, 25, inner - 206, 17, white, "left");
            label(row, "price", r.price, inner - 196, 25, 188, 17, tone(r.tone), "right");
            hover(row, r.detail);
        }
    }
    function empty(text:String):Void { label(content, "empty", text, 22, (h + 82) / 2 - 15, w - 44, 20, muted, "center"); }
    function showTip(text:String):Void {
        hideTip();
        tooltip = clip(root, "tooltip", 0, 0);
        var s:Number = panel._xscale / 100;
        tooltip._xscale = tooltip._yscale = s * 100;
        var f:TextField = label(tooltip, "text", "", 9, 6, 360, 17, white, "left");
        f.multiline = true; f.autoSize = "left"; f.text = text;
        var width:Number = Math.max(24, Math.min(360, f.textWidth + 5, (Stage.width - 16) / s - 18));
        f.wordWrap = true; f._width = width; f.text = text;
        rect(tooltip, 0, 0, width + 18, f._height + 12, 0x0E1511, 100);
        line(tooltip, 0, 0, width + 18, 0, gold);
        tooltip._x = Math.max(8, Math.min(root._xmouse + 12, Stage.width - (width + 18) * s - 8));
        tooltip._y = Math.max(8, Math.min(root._ymouse + 20, Stage.height - (f._height + 12) * s - 8));
    }
    function hideTip():Void {
        if (tooltip != undefined) tooltip.removeMovieClip();
        tooltip = undefined;
    }
    function onMouseUp():Void {
        if (dragging || resizing) {
            panel.stopDrag(); delete root.onEnterFrame;
            dragging = resizing = false; remember();
        }
    }
    function onMouseWheel(delta:Number):Void {
        if (panel == undefined || choosing || !panel.hitTest(root._xmouse, root._ymouse, false)) return;
        action("page", delta > 0 ? -1 : 1);
    }
    function onKeyDown():Void {
        if (data == undefined) return;
        switch (Key.getCode()) {
        case 27: command("close"); break;
        case 9: command(Key.isDown(Key.SHIFT) ? "prevTab" : "nextTab"); break;
        case 49: action("tab", 0); break;
        case 50: action("tab", 1); break;
        case 51: action("tab", 2); break;
        case 37: command("left"); break;
        case 39: command("right"); break;
        case 33: command("up"); break;
        case 34: command("down"); break;
        }
    }
    function command(key:String):Void {
        if (data == undefined) return;
        if (key == "close") { if (choosing) { choosing = false; draw(); } else native("close", null); }
        else if (key == "nextTab") action("tab", (tab + 1) % 3);
        else if (key == "prevTab") action("tab", (tab + 2) % 3);
        else if (key == "markets") action("tab", 0);
        else if (key == "routes") action("tab", 1);
        else if (key == "rumors") action("tab", 2);
        else if (key == "left" || key == "right") action(tab == 0 ? "holdStep" : "page", key == "left" ? -1 : 1);
        else if (key == "up" || key == "down") action("page", key == "up" ? -1 : 1);
    }
    function action(kind:String, value:Object):Void {
        if (kind == "close") { native("close", null); return; }
        if (kind == "lock") { prefs.locked = !prefs.locked; remember(); }
        else if (kind == "tab") { tab = Number(value); page = 0; choosing = false; }
        else if (kind == "hold") { hold = Number(value); page = 0; choosing = false; }
        else if (kind == "holdStep") {
            hold = (hold + Number(value) + data.markets.length) % data.markets.length;
            page = 0; choosing = false;
        }
        else if (kind == "page") page = Math.max(0, Math.min(pages - 1, page + Number(value)));
        else if (kind == "chooser") choosing = !choosing;
        else if (kind == "pinnedOnly") { pinnedOnly = !pinnedOnly; page = 0; }
        else if (kind == "pin") {
            for (var i:Number = 0; i < data.routes.length; ++i)
                if (data.routes[i].key == value) data.routes[i].pinned = !data.routes[i].pinned;
            native("pin", value);
        }
        draw();
    }
}
