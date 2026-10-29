import argparse
import struct
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Button, Slider, TextBox


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', nargs='?', default='simulation.bin')
    parser.add_argument('--save', help='Save a static image instead of opening the viewer')
    args = parser.parse_args()
    path = Path(args.file)
    with path.open('rb') as stream:
        header = stream.read(20)
    if len(header) != 20:
        parser.error('Incomplete header')
    count, duration, step = struct.unpack('<idd', header)
    if count < 1 or not np.isfinite([duration, step]).all() or step <= 0:
        parser.error('Invalid header')
    frames_data = []
    counts = []
    with path.open('rb') as stream:
        stream.seek(20)
        previous = -np.inf
        while True:
            frame_header = stream.read(12)
            if not frame_header:
                break
            if len(frame_header) != 12:
                parser.error('Incomplete step header')
            time, active = struct.unpack('<di', frame_header)
            if not np.isfinite(time) or time <= previous or not 1 <= active <= count:
                parser.error('Invalid step; regenerate the binary with the new output block')
            if counts and active > counts[-1]:
                parser.error('Body count increased unexpectedly')
            payload = stream.read(active * 16)
            if len(payload) != active * 16:
                parser.error('Incomplete positions')
            xy = np.frombuffer(payload, dtype='<f8').reshape(active, 2)
            if not np.isfinite(xy).all():
                parser.error('Nonfinite positions')
            row = np.full((count, 3), np.nan)
            row[:, 0] = time
            row[:active, 1:3] = xy
            frames_data.append(row)
            counts.append(active)
            previous = time
    if not frames_data:
        parser.error('No saved steps')
    records = np.asarray(frames_data)
    del frames_data
    counts = np.asarray(counts)
    frames = len(records)
    times = records[:, 0, 0]
    low = np.nanmin(records[:, :, 1:3], axis=(0, 1))
    high = np.nanmax(records[:, :, 1:3], axis=(0, 1))
    changes = np.flatnonzero(np.r_[False, counts[1:] != counts[:-1]])

    fig, ax = plt.subplots(figsize=(10, 8))
    fig.subplots_adjust(bottom=.25, top=.9)
    center = (low + high) / 2
    span = max(float((high - low).max()), 1.0)
    ax.set(xlim=(center[0]-.55*span, center[0]+.55*span),
           ylim=(center[1]-.55*span, center[1]+.55*span),
           xlabel='X (meters)', ylabel='Y (meters)')
    ax.set_aspect('equal')
    ax.grid(True, alpha=.25)
    trails, markers = [], []
    for body in range(count):
        line, = ax.plot([], [], lw=1, label=f'Body {body}')
        marker, = ax.plot([], [], 'o', color=line.get_color(), markersize=7)
        trails.append(line)
        markers.append(marker)
    ax.legend()
    title = ax.set_title('')
    status = fig.text(.12, .02, '', fontsize=9)
    state = {'index': 0, 'playing': False, 'updating': False}
    slider = Slider(fig.add_axes([.16, .15, .69, .03]), 'Time (s)',
                    float(times[0]), float(times[-1]) if frames > 1 else float(times[0])+1,
                    valinit=float(times[0]))
    button = Button(fig.add_axes([.12, .065, .14, .05]), 'Play')
    back = Button(fig.add_axes([.28, .065, .10, .05]), 'Previous')
    forward = Button(fig.add_axes([.40, .065, .10, .05]), 'Next')
    box = TextBox(fig.add_axes([.68, .065, .18, .05]), 'Jump to (s) ', initial='0')

    def pause():
        state['playing'] = False
        button.label.set_text('Play')

    def nearest(value):
        k = int(np.searchsorted(times, value))
        if k == 0:
            return 0
        if k >= frames:
            return frames - 1
        return k if times[k]-value < value-times[k-1] else k-1

    def show(index):
        state['index'] = int(index)
        # Limit trail rendering, but always show the exact selected saved state.
        # Indices may change after merging. Start new trails after each count change.
        history_start = int(changes[np.searchsorted(changes, index, side='right')-1]) if np.searchsorted(changes, index, side='right') else 0
        stride = max(1, (index-history_start+1)//10000)
        indices = np.unique(np.r_[np.arange(history_start, index+1, stride), index])
        xy = np.asarray(records[indices, :, 1:3])
        for body in range(count):
            trails[body].set_data(xy[:, body, 0], xy[:, body, 1])
            markers[body].set_data([records[index, body, 1]], [records[index, body, 2]])
        title.set_text(f'Time: {times[index]:,.6f} s | Bodies: {counts[index]} | Step {index+1:,}/{frames:,}')
        state['updating'] = True
        try:
            slider.set_val(float(times[index]))
        finally:
            state['updating'] = False
        fig.canvas.draw_idle()

    def seek(value):
        if state['updating']:
            return
        pause()
        show(nearest(value))
        status.set_text('Paused at nearest saved time. Space: play/pause; arrows: one saved step.')

    def jump(text):
        try:
            value = float(text)
            if not np.isfinite(value):
                raise ValueError
        except ValueError:
            status.set_text('Enter a finite time in seconds.')
            fig.canvas.draw_idle()
            return
        pause()
        show(nearest(value))
        status.set_text(f'Requested {value:g} s; showing nearest saved time {times[state["index"]]:g} s.')
        fig.canvas.draw_idle()

    def toggle(event=None):
        if not state['playing'] and state['index'] == frames-1:
            show(0)
        state['playing'] = not state['playing']
        button.label.set_text('Pause' if state['playing'] else 'Play')
        fig.canvas.draw_idle()

    def move(amount):
        pause()
        show(max(0, min(frames-1, state['index']+amount)))

    def key(event):
        if event.inaxes is box.ax:
            return
        if event.key == ' ':
            toggle()
        elif event.key == 'right':
            move(1)
        elif event.key == 'left':
            move(-1)

    def tick():
        if state['playing']:
            show(min(frames-1, state['index']+max(1, (frames-1)//1800)))
            if state['index'] == frames-1:
                pause()
                fig.canvas.draw_idle()

    slider.on_changed(seek)
    box.on_submit(jump)
    button.on_clicked(toggle)
    back.on_clicked(lambda event: move(-1))
    forward.on_clicked(lambda event: move(1))
    fig.canvas.mpl_connect('key_press_event', key)
    timer = fig.canvas.new_timer(interval=33)
    timer.add_callback(tick)
    show(0)
    status.set_text('Space: play/pause. Arrows: one saved step. Jump uses nearest saved time.')
    print(f'{frames:,} saved steps, {count} bodies; {times[0]:g} to {times[-1]:g} seconds.')
    if args.save:
        show(frames-1)
        fig.savefig(args.save, dpi=160)
        plt.close(fig)
    else:
        timer.start()
        plt.show()
        timer.stop()


if __name__ == '__main__':
    main()
