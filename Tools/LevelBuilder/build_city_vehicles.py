# Place abandoned/parked civilian vehicles through the city (Wave 6 — a contested
# evacuation reads as lived-in: parked cars along curbs + abandoned roadblocks at
# intersections). Uses the Fab "Vehicle Variety Pack" static body meshes already in
# the project. Placed on road centerlines/edges (level-plan guarantees open) so
# nothing clips into buildings.
#
# ADDITIVE + IDEMPOTENT under "Dressing/CityVehicles". Run headless (kill UnrealEditor first):
#   UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...build_city_vehicles.py" -d3d11
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sh_lib as S  # noqa: E402

FOLDER = "Dressing/CityVehicles"
U = S.PLAN["urban"]
RNG = S.RNG

CARS = [
    "/Game/VehicleVarietyPack/Meshes/SM_Hatchback",
    "/Game/VehicleVarietyPack/Meshes/SM_Pickup",
    "/Game/VehicleVarietyPack/Meshes/SM_SportsCar",
    "/Game/VehicleVarietyPack/Meshes/SM_SUV",
    "/Game/VehicleVarietyPack/Meshes/SM_Truck_Box",
]


def place(x, y, yaw, label, folder):
    z = S.trace_z(x, y, None)
    # The urban ground plateau sits ~11m up, so only z above ~25m is a rooftop.
    if z is None or z > 2500:   # hit a building roof -> skip (don't put a car on a building)
        return False
    mesh = RNG.choice(CARS)
    a = S.spawn_sm(mesh, x, y, yaw=yaw, z=z, ground=True, sink=2.0,
                   label=label, folder=folder)
    if a:
        # Cull distant cars (dev-box perf). StaticMeshComponent uses the
        # LDMaxDrawDistance property (set_cull_distances is HISM-only).
        try:
            a.static_mesh_component.set_editor_property("ld_max_draw_distance", 70000.0)
        except Exception:  # noqa: BLE001
            pass
    return bool(a)


def main():
    S.open_map()
    S.clear_folder_actors(FOLDER)
    avenues = U["avenues"]   # E-W roads (run along X) at constant Y
    streets = U["streets"]   # N-S roads (run along Y) at constant X
    x0, x1 = U["x0"] + 3000, U["x1"] - 3000
    y0, y1 = U["y1"] + 3000, U["y0"] - 3000  # urban Y band (y1<y0)

    parked = 0
    # Parked along avenues (face +/-X), near a curb (offset in Y).
    for ay in avenues:
        n = RNG.randint(2, 4)
        for _ in range(n):
            px = RNG.uniform(x0, x1)
            py = ay + RNG.choice([-280.0, 280.0]) + RNG.uniform(-40, 40)
            yaw = RNG.choice([0.0, 180.0]) + RNG.uniform(-3, 3)
            if place(px, py, yaw, f"Car_Ave_{int(px)}_{int(py)}", FOLDER + "/Parked"):
                parked += 1
    # Parked along streets (face +/-Y), near a curb (offset in X).
    for sx in streets:
        n = RNG.randint(1, 3)
        for _ in range(n):
            px = sx + RNG.choice([-280.0, 280.0]) + RNG.uniform(-40, 40)
            py = RNG.uniform(y0, y1)
            yaw = RNG.choice([90.0, 270.0]) + RNG.uniform(-3, 3)
            if place(px, py, yaw, f"Car_St_{int(px)}_{int(py)}", FOLDER + "/Parked"):
                parked += 1
    S.log(f"parked cars: {parked}")

    # Abandoned roadblock cars at a few intersections — askew (fled in a hurry).
    inter = [(sx, ay) for sx in streets for ay in avenues]
    RNG.shuffle(inter)
    blocks = 0
    for (sx, ay) in inter[:6]:
        for _ in range(RNG.randint(1, 2)):
            bx = sx + RNG.uniform(-300, 300)
            by = ay + RNG.uniform(-300, 300)
            yaw = RNG.uniform(0, 360)   # abandoned at any angle
            if place(bx, by, yaw, f"Car_Block_{int(bx)}_{int(by)}", FOLDER + "/Roadblocks"):
                blocks += 1
    S.log(f"abandoned roadblock cars: {blocks}")

    S.save_map()
    with open(os.path.join(S.OUT, "city_vehicles_result.txt"), "w", encoding="utf-8") as f:
        f.write("city vehicles: %d parked + %d roadblocks\n" % (parked, blocks))
    S.log("city vehicles pass complete, map saved")


if __name__ == "__main__":
    main()
