// Vigia o descanso do Mac e chama tools/cydex_power.sh sleep|wake.
//
//   swiftc -O tools/cydex_power_watch.swift -o tools/cydex-power-watch
//
// Roda como LaunchAgent (tools/cydex-power.plist.example). O sleepwatcher não
// recebe mais os eventos de tela no macOS atual, e as notificações do
// NSWorkspace só chegam com o NSApplication iniciado (sem ele, nada chega).
import AppKit

let script = URL(fileURLWithPath: CommandLine.arguments[0])
  .deletingLastPathComponent().appendingPathComponent("cydex_power.sh").path

func send(_ what: String) {
  print("cydex-power: \(what)")
  fflush(stdout)
  let p = Process()
  p.executableURL = URL(fileURLWithPath: "/bin/sh")
  p.arguments = [script, what]
  try? p.run()  // não espera: o wake tenta por até ~20 s
}

let app = NSApplication.shared
app.setActivationPolicy(.prohibited)
let nc = NSWorkspace.shared.notificationCenter
let events: [(Notification.Name, String)] = [
  (NSWorkspace.screensDidSleepNotification, "sleep"),
  (NSWorkspace.willSleepNotification, "sleep"),
  (NSWorkspace.screensDidWakeNotification, "wake"),
  (NSWorkspace.didWakeNotification, "wake"),
]
for (name, what) in events {
  nc.addObserver(forName: name, object: nil, queue: .main) { _ in send(what) }
}
app.run()
