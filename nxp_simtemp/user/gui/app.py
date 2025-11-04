#!/usr/bin/env python3
"""GUI for temperature sensor using shared backend"""
import tkinter as tk
from tkinter import ttk, messagebox
import threading
from queue import Queue, Empty
from datetime import datetime

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

from backend.simtemp_interface import SimTempSensorInterface

class TempSensorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Temperature Sensor Monitor")
        self.sensor = SimTempSensorInterface()
        self.monitoring = False
        self.error_queue = Queue()
        
        # build UI and load sensor defaults
        self._create_widgets()
        self._load_current_config()

        # plotting data
        self.times = []   # elapsed seconds since start
        self.temps = []
        self.alerts = []
        self._start_time = None
        self._init_plot()
    
    def _create_widgets(self):
        # Configuration frame
        config_frame = ttk.LabelFrame(self.root, text="Configuration", padding=10)
        config_frame.grid(row=0, column=0, padx=10, pady=10, sticky="ew")
        
        ttk.Label(config_frame, text="Sampling (ms):").grid(row=0, column=0, sticky="w")
        self.sampling_var = tk.StringVar()
        ttk.Entry(config_frame, textvariable=self.sampling_var, width=10).grid(row=0, column=1, padx=5)
        
        ttk.Label(config_frame, text="Threshold (°C):").grid(row=1, column=0, sticky="w")
        self.threshold_var = tk.StringVar()
        ttk.Entry(config_frame, textvariable=self.threshold_var, width=10).grid(row=1, column=1, padx=5)
        
        ttk.Label(config_frame, text="Mode:").grid(row=2, column=0, sticky="w")
        self.mode_var = tk.StringVar()
        mode_combo = ttk.Combobox(config_frame, textvariable=self.mode_var, 
                                   values=['normal', 'noisy', 'ramp'], width=10)
        mode_combo.grid(row=2, column=1, padx=5)
        
        ttk.Button(config_frame, text="Apply", command=self._apply_config).grid(row=3, column=0, columnspan=2, pady=5)
        
        # Monitor frame
        monitor_frame = ttk.LabelFrame(self.root, text="Monitor", padding=10)
        monitor_frame.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")
        
        self.start_btn = ttk.Button(monitor_frame, text="Start Monitoring", command=self._start_monitoring)
        self.start_btn.pack(pady=5)
        
        self.stop_btn = ttk.Button(monitor_frame, text="Stop Monitoring", command=self._stop_monitoring, state='disabled')
        self.stop_btn.pack(pady=5)

        # Plot area
        self.plot_frame = ttk.Frame(monitor_frame)
        self.plot_frame.pack(fill='both', expand=True, pady=5)

        # Readings display
        self.readings_text = tk.Text(monitor_frame, height=6, width=50)
        self.readings_text.pack(pady=5)

        self.root.rowconfigure(1, weight=1)
        self.root.columnconfigure(0, weight=1)
    
    def _load_current_config(self):
        self.sampling_var.set(str(self.sensor.get_sampling_ms()))
        self.threshold_var.set(f"{self.sensor.get_threshold_c():.1f}")
        self.mode_var.set(self.sensor.get_mode())
    
    def _apply_config(self):
        try:
            self.sensor.set_sampling_ms(int(self.sampling_var.get()))
            self.sensor.set_threshold_c(float(self.threshold_var.get()))
            self.sensor.set_mode(self.mode_var.get())
            self.readings_text.insert(tk.END, "✓ Configuration applied\n")
            # update threshold line on plot
            self._update_plot()
        except ValueError:
            self.readings_text.insert(tk.END, "✗ Invalid configuration\n")
    
    def _start_monitoring(self):
        self.monitoring = True
        self.start_btn.config(state='disabled')
        self.stop_btn.config(state='normal')
        self.readings_text.delete(1.0, tk.END)
        
        # Run monitoring in background thread
        thread = threading.Thread(target=self._monitor_loop, daemon=True)
        thread.start()
        
        # Start error checking
        self.root.after(100, self._check_errors)
    
    def _stop_monitoring(self):
        self.monitoring = False
        self.start_btn.config(state='normal')
        self.stop_btn.config(state='disabled')
    
    def _monitor_loop(self):
        if not self.sensor.open_device():
            self.error_queue.put("Failed to open device. Run the GUI as sudo or check if the kernel module is loaded")
            return
            
        try:
            while self.monitoring:
                reading = self.sensor.poll_reading()
                if reading:
                    # Update GUI from background thread (use after_idle for thread safety)
                    self.root.after_idle(self._add_reading, reading)
        except Exception as e:
            self.error_queue.put(f"Monitoring error: {str(e)}")
        finally:
            self.sensor.close_device()
    
    def _add_reading(self, text):
        # `text` is a SensorReading object; show text and update plot
        try:
            reading = text
            self.readings_text.insert(tk.END, str(reading) + "\n")
            self.readings_text.see(tk.END)

            # parse timestamp (ISO 8601 with trailing Z)
            try:
                dt = datetime.fromisoformat(reading.timestamp.replace('Z', '+00:00'))
            except Exception:
                dt = datetime.utcnow()

            if self._start_time is None:
                self._start_time = dt

            elapsed = (dt - self._start_time).total_seconds()
            self.times.append(elapsed)
            self.temps.append(reading.temp_c)
            self.alerts.append(bool(reading.is_alert))

            # keep recent samples only
            MAX_POINTS = 200
            if len(self.times) > MAX_POINTS:
                self.times = self.times[-MAX_POINTS:]
                self.temps = self.temps[-MAX_POINTS:]
                self.alerts = self.alerts[-MAX_POINTS:]

            self._update_plot()
        except Exception as e:
            # fallback: just display raw
            self.readings_text.insert(tk.END, str(text) + "\n")
            self.readings_text.see(tk.END)
        
    def _check_errors(self):
        """Check for errors from the background thread"""
        try:
            while True:
                error = self.error_queue.get_nowait()
                messagebox.showerror("Error", error)
                self._stop_monitoring()  # Stop monitoring on error
                return
        except Empty:
            if self.monitoring:
                # Continue checking while monitoring is active
                self.root.after(100, self._check_errors)

    # --- plotting helpers -------------------------------------------------
    def _init_plot(self):
        """Create matplotlib Figure and initial empty plot embedded in Tk."""
        self.fig = Figure(figsize=(6, 3))
        self.ax = self.fig.add_subplot(111)
        self.ax.set_title('Temperature')
        self.ax.set_xlabel('Time (s)')
        self.ax.set_ylabel('Temperature (°C)')
        self.ax.grid(True)

        # initial empty plot
        self.ax.plot([], [], '-o', color='tab:blue')
        # threshold line placeholder
        thresh = self.sensor.get_threshold_c()
        self.threshold_line = self.ax.axhline(thresh, color='orange', linestyle='--', label='threshold')

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_frame)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill='both', expand=True)

    def _update_plot(self):
        """Redraw the plot from current data (safe to call from main thread)."""
        if not hasattr(self, 'ax'):
            return

        # copy lists to avoid mutation during draw
        xs = list(self.times)
        ys = list(self.temps)
        alerts = list(self.alerts)

        self.ax.clear()
        self.ax.grid(True)
        self.ax.set_title('Temperature')
        self.ax.set_xlabel('Time (s)')
        self.ax.set_ylabel('Temperature (°C)')

        if xs and ys:
            self.ax.plot(xs, ys, '-o', color='tab:blue', markersize=5, label='temp')

            # highlight alert points in red
            alert_x = [x for x, a in zip(xs, alerts) if a]
            alert_y = [y for y, a in zip(ys, alerts) if a]
            if alert_x:
                self.ax.scatter(alert_x, alert_y, color='red', zorder=5, label='alert')

        # threshold
        try:
            thresh = float(self.sensor.get_threshold_c())
            self.ax.axhline(thresh, color='orange', linestyle='--', label='threshold')
        except Exception:
            pass

        self.ax.legend(loc='upper right')
        self.fig.tight_layout()
        self.canvas.draw()

def main():
    root = tk.Tk()
    app = TempSensorGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
