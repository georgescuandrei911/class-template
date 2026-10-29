import argparse
import struct
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', nargs='?', default='simulation.bin')
    parser.add_argument('--max-points', type=int, default=20000,
                        help='Maximum plotted points per body (default: 20000)')
    parser.add_argument('--save', help='Save a figure instead of opening a window')
    args = parser.parse_args()
    path = Path(args.file)
    if args.max_points < 2:
        parser.error('--max-points must be at least 2')
    with path.open('rb') as stream:
        header = stream.read(20)
    if len(header) != 20:
        parser.error('Incomplete binary header')
    count, max_time, step = struct.unpack('<idd', header)
    if count <= 0 or not np.isfinite([max_time, step]).all() or step <= 0:
        parser.error('Invalid simulation settings')
    payload = path.stat().st_size - 20
    if payload <= 0 or payload % (72 * count):
        parser.error('Incomplete time step or incompatible binary layout')
    frames = payload // (72 * count)
    records = np.memmap(path, dtype='<f8', mode='r', offset=20,
                        shape=(frames, count, 9))
    indices = np.unique(np.linspace(0, frames - 1,
                                   min(frames, args.max_points), dtype=np.int64))
    sampled = np.asarray(records[indices])
    if not np.isfinite(sampled).all():
        parser.error('Sampled records contain NaN or infinity')
    if not np.all(sampled[:, :, 0] == sampled[:, :1, 0]):
        parser.error('Bodies have mismatched timestamps')

    fig, ax = plt.subplots(figsize=(10, 8))
    for body in range(count):
        x, y = sampled[:, body, 3], sampled[:, body, 4]
        line, = ax.plot(x, y, linewidth=1.2, label=f'Body {body + 1}')
        ax.plot(x[0], y[0], 'o', color=line.get_color(), markersize=6)
        ax.plot(x[-1], y[-1], 'x', color=line.get_color(), markersize=8)
    ax.set(xlabel='X (meters)', ylabel='Y (meters)',
           title=f'Trajectories | {count} bodies | dt = {step:g} s\n'
                 f'{sampled[0, 0, 0]:g} to {sampled[-1, 0, 0]:g} s '
                 '(circle = start, cross = end)')
    ax.set_aspect('equal', adjustable='datalim')
    ax.grid(True, alpha=0.25)
    ax.legend()
    fig.tight_layout()
    print(f'{frames:,} saved steps; plotting {len(indices):,} points per body.')
    if args.save:
        fig.savefig(args.save, dpi=160)
        plt.close(fig)
    else:
        plt.show()


if __name__ == '__main__':
    main()
