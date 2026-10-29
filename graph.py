import argparse
import struct
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


def main():
    parser = argparse.ArgumentParser(
        description="Animate planets and the force on observer 0."
    )
    parser.add_argument("file", nargs="?", default="simulation.bin")
    parser.add_argument("--frames", type=int, default=1200)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--save", help="Save an animated GIF")
    args = parser.parse_args()

    path = Path(args.file)

    with path.open("rb") as stream:
        header = stream.read(20)

    if len(header) != 20:
        parser.error("Incomplete file header.")

    count, max_time, time_step = struct.unpack("<idd", header)

    if (
        count < 2
        or not np.isfinite([max_time, time_step]).all()
        or time_step <= 0
        or args.frames < 2
        or args.fps <= 0
    ):
        parser.error("Invalid simulation or animation settings.")

    # Your C++ writes, per simulation step:
    #   time, force_on_observer.x, force_on_observer.y
    #   then N records: time, mass, radius, px, py, vx, vy, ax, ay
    width = 3 + 9 * count
    bytes_per_step = width * 8
    payload = path.stat().st_size - 20

    if payload <= 0 or payload % bytes_per_step:
        parser.error(
            "Incomplete file or incompatible format. "
            "Finish the simulation before opening the file."
        )

    saved_steps = payload // bytes_per_step

    records = np.memmap(
        path,
        dtype="<f8",
        mode="r",
        offset=20,
        shape=(saved_steps, width),
    )

    # Sample large files to keep the animation responsive.
    indices = np.unique(
        np.linspace(
            0,
            saved_steps - 1,
            min(saved_steps, args.frames),
            dtype=np.int64,
        )
    )

    data = np.array(records[indices])
    times = data[:, 0]
    forces = data[:, 1:3]
    bodies = data[:, 3:].reshape(-1, count, 9)

    if not np.isfinite(data).all():
        parser.error("Sampled data contains NaN or infinity.")

    if np.any(np.diff(times) <= 0):
        parser.error("Timestamps are not increasing.")

    if not np.all(bodies[:, :, 0] == times[:, None]):
        parser.error("Body timestamps do not match force timestamps.")

    positions = bodies[:, :, 3:5]
    magnitude = np.hypot(forces[:, 0], forces[:, 1])

    fig, (orbit_ax, force_ax) = plt.subplots(
        1, 2, figsize=(14, 6)
    )

    minimum = positions.min(axis=(0, 1))
    maximum = positions.max(axis=(0, 1))
    center = (minimum + maximum) / 2
    span = max(float((maximum - minimum).max()), 1.0)

    orbit_ax.set(
        xlim=(center[0] - 0.6 * span, center[0] + 0.6 * span),
        ylim=(center[1] - 0.6 * span, center[1] + 0.6 * span),
        xlabel="X (meters)",
        ylabel="Y (meters)",
        title="Planet trajectories",
    )
    orbit_ax.set_aspect("equal")

    trails = []
    markers = []

    for body in range(count):
        label = "Observer" if body == 0 else f"Planet {body}"

        trail, = orbit_ax.plot(
            [], [], linewidth=1, label=label
        )

        marker, = orbit_ax.plot(
            [],
            [],
            "s" if body == 0 else "o",
            color=trail.get_color(),
            markersize=7,
        )

        trails.append(trail)
        markers.append(marker)

    orbit_ax.legend(loc="upper right")

    # Force arrow starts at observer 0.
    # A constant scale preserves relative changes in magnitude.
    maximum_force = float(magnitude.max())
    arrow_scale = (
        0.2 * span / maximum_force if maximum_force > 0 else 1.0
    )

    arrow = orbit_ax.quiver(
        [positions[0, 0, 0]],
        [positions[0, 0, 1]],
        [0.0],
        [0.0],
        angles="xy",
        scale_units="xy",
        scale=1,
        color="black",
        zorder=5,
    )

    orbit_ax.text(
        0.02,
        0.02,
        "Black arrow: observer net force\n"
        "(fixed visual scale)",
        transform=orbit_ax.transAxes,
    )

    force_ax.plot(
        times, magnitude, color="purple", linewidth=1
    )

    cursor = force_ax.axvline(
        times[0], color="black", alpha=0.5
    )
    force_point, = force_ax.plot([], [], "ko")

    force_ax.set(
        xlabel="Time (seconds)",
        ylabel="Net force magnitude (newtons)",
        title="Force on observer",
    )

    for axis in (orbit_ax, force_ax):
        axis.grid(True, alpha=0.25)

    title = fig.suptitle("")

    def update(frame):
        for body in range(count):
            trails[body].set_data(
                positions[:frame + 1, body, 0],
                positions[:frame + 1, body, 1],
            )

            markers[body].set_data(
                [positions[frame, body, 0]],
                [positions[frame, body, 1]],
            )

        arrow.set_offsets(positions[frame, :1])
        arrow.set_UVC(
            [forces[frame, 0] * arrow_scale],
            [forces[frame, 1] * arrow_scale],
        )

        cursor.set_xdata([times[frame], times[frame]])
        force_point.set_data(
            [times[frame]], [magnitude[frame]]
        )

        title.set_text(
            f"Time: {times[frame]:,.1f} s | "
            f"Observer force: {magnitude[frame]:.6g} N"
        )

        return trails + markers + [
            arrow, cursor, force_point, title
        ]

    fig.tight_layout(rect=(0, 0, 1, 0.94))

    animation = FuncAnimation(
        fig,
        update,
        frames=len(times),
        interval=1000 / args.fps,
        blit=False,
        repeat=True,
    )

    print(
        f"{saved_steps:,} saved steps; "
        f"{len(times):,} animation frames."
    )

    if args.save:
        animation.save(
            args.save,
            writer="pillow",
            fps=args.fps,
        )
        plt.close(fig)
    else:
        plt.show()


if __name__ == "__main__":
    main()