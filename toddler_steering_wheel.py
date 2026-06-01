import os
import sys

import cadquery as cq
from cadquery import exporters


# -----------------------------
# Main dimensions
# -----------------------------
wheel_outer_diameter = 180.0
wheel_inner_diameter = 118.0
wheel_thickness = 16.0
grip_roundover_radius = 7.0

hub_diameter = 78.0
hub_thickness = 28.0
hub_roundover_radius = 2.0

spoke_count = 3
spoke_width = 20.0
spoke_thickness = 14.0
spoke_roundover_radius = 2.0

# Modular hub with removable electronics cartridge
include_modular_cartridge_hub = True
hub_rear_lip_thickness = 3.0
hub_rear_lip_radial_width = 3.0
cartridge_outer_diameter = 62.0
cartridge_fit_clearance = 0.35
cartridge_key_flat_depth = 6.0
cartridge_key_angle = 0.0
cartridge_floor_thickness = 2.4
cartridge_wall_thickness = 2.4
cartridge_has_front_cap = False
cartridge_front_face_thickness = 2.4
cartridge_lid_thickness = 2.4
cartridge_lid_overlap_depth = 4.0
cartridge_lid_clearance = 0.3

# Legacy electronics pocket in back of hub
include_electronics_pocket = False
include_pocket_footprint_inset = False
pocket_width = 34.0
pocket_height = 48.0
pocket_depth = 10.0
pocket_footprint_inset_depth = 1.2
pocket_corner_radius = 4.0

# Cable/sensor hole through the center
include_center_hole = False
wire_hole_diameter = 8.0

# Optional mounting holes for a small board or sensor
include_mounting_holes = False
mount_hole_spacing_x = 24.0
mount_hole_spacing_y = 34.0
mount_hole_diameter = 2.6
mount_hole_depth = 7.0

# Export filenames
step_filename = "toddler_steering_wheel.step"
stl_filename = "toddler_steering_wheel.stl"
cartridge_step_filename = "toddler_steering_wheel_cartridge.step"
cartridge_stl_filename = "toddler_steering_wheel_cartridge.stl"
cartridge_lid_step_filename = "toddler_steering_wheel_cartridge_lid.step"
cartridge_lid_stl_filename = "toddler_steering_wheel_cartridge_lid.stl"


# -----------------------------
# Derived values
# -----------------------------
outer_radius = wheel_outer_diameter / 2
inner_radius = wheel_inner_diameter / 2
hub_radius = hub_diameter / 2

spoke_length = inner_radius - hub_radius + 8.0
spoke_center_offset = hub_radius + spoke_length / 2 - 4.0
cartridge_cavity_diameter = cartridge_outer_diameter + 2.0 * cartridge_fit_clearance
cartridge_rear_stop_diameter = cartridge_outer_diameter - 2.0 * hub_rear_lip_radial_width
cartridge_cavity_depth = hub_thickness - hub_rear_lip_thickness
cartridge_height = cartridge_cavity_depth - cartridge_fit_clearance
cartridge_inner_diameter = cartridge_outer_diameter - 2.0 * cartridge_wall_thickness
cartridge_inner_key_flat_depth = cartridge_key_flat_depth
cartridge_inner_depth = max(
    cartridge_height
    - cartridge_floor_thickness
    - (cartridge_front_face_thickness if cartridge_has_front_cap else 0.0),
    4.0,
)
cartridge_lid_insert_diameter = (
    cartridge_inner_diameter - 2.0 * cartridge_lid_clearance
)
cartridge_lid_insert_flat_depth = cartridge_key_flat_depth
cartridge_lid_insert_depth = min(
    max(cartridge_lid_overlap_depth, 1.5),
    cartridge_inner_depth - 0.5,
)


def make_cylinder_with_flat(diameter, height, flat_depth, angle=90.0):
    """Create a cylinder with one flat to key its rotational orientation."""
    radius = diameter / 2.0
    solid = cq.Workplane("XY").circle(radius).extrude(height)

    if flat_depth <= 0:
        return solid

    flat_line_y = radius - flat_depth
    cutter = (
        cq.Workplane("XY")
        .box(diameter * 3.0, diameter * 2.0, height + 2.0)
        .translate((0, flat_line_y + diameter, height / 2.0))
    )
    keyed = solid.cut(cutter)
    return keyed.rotate((0, 0, 0), (0, 0, 1), angle - 90.0)


# -----------------------------
# Wheel rim
# -----------------------------
print("Building rim...")
rim = (
    cq.Workplane("XY")
    .circle(outer_radius)
    .circle(inner_radius)
    .extrude(wheel_thickness)
    .edges("%CIRCLE")
    .fillet(grip_roundover_radius)
)


# -----------------------------
# Center hub
# -----------------------------
print("Building hub...")
hub = (
    cq.Workplane("XY")
    .circle(hub_radius)
    .extrude(hub_thickness)
    .edges("%CIRCLE")
    .fillet(hub_roundover_radius)
)


# -----------------------------
# Spokes
# -----------------------------
print("Building spokes...")
spokes = cq.Workplane("XY")

for i in range(spoke_count):
    angle = i * 360.0 / spoke_count

    spoke = (
        cq.Workplane("XY")
        .box(spoke_length, spoke_width, spoke_thickness)
        .translate((spoke_center_offset, 0, spoke_thickness / 2))
        .rotate((0, 0, 0), (0, 0, 1), angle)
        .edges("|Z")
        .fillet(spoke_roundover_radius)
    )

    spokes = spokes.union(spoke)


# -----------------------------
# Combine main body
# -----------------------------
print("Combining body...")
result = rim.union(spokes).union(hub)
cartridge_result = None
cartridge_lid_result = None


# -----------------------------
# Modular cartridge hub
# The wheel gets a keyed cavity from the back.
# A separate starter cartridge shell is exported for iteration.
# -----------------------------
if include_modular_cartridge_hub:
    print("Cutting keyed through-cartridge cavity...")
    front_cavity_cut = make_cylinder_with_flat(
        cartridge_cavity_diameter,
        cartridge_cavity_depth + 0.2,
        cartridge_key_flat_depth + cartridge_fit_clearance,
        cartridge_key_angle,
    ).translate((0, 0, hub_thickness - cartridge_cavity_depth))

    rear_stop_opening_cut = (
        cq.Workplane("XY")
        .circle(cartridge_rear_stop_diameter / 2.0)
        .extrude(hub_rear_lip_thickness + 0.2)
        .translate((0, 0, -0.1))
    )

    result = result.cut(front_cavity_cut).cut(rear_stop_opening_cut)

    print("Building starter cartridge shell...")
    cartridge_outer = make_cylinder_with_flat(
        cartridge_outer_diameter,
        cartridge_height,
        cartridge_key_flat_depth,
        cartridge_key_angle,
    )
    inner_start_z = cartridge_floor_thickness
    cartridge_inner = (
        make_cylinder_with_flat(
            cartridge_inner_diameter,
            cartridge_inner_depth + 0.2,
            cartridge_inner_key_flat_depth,
            cartridge_key_angle,
        )
        .translate((0, 0, inner_start_z))
    )

    cartridge_result = cartridge_outer.cut(cartridge_inner)

    print("Building starter cartridge lid...")
    lid_plate = (
        cq.Workplane("XY")
        .circle(cartridge_outer_diameter / 2.0)
        .extrude(cartridge_lid_thickness)
    )
    lid_insert = make_cylinder_with_flat(
        cartridge_lid_insert_diameter,
        cartridge_lid_insert_depth,
        cartridge_lid_insert_flat_depth,
        cartridge_key_angle,
    )
    lid_insert = lid_insert.translate((0, 0, -cartridge_lid_insert_depth))
    cartridge_lid_result = lid_plate.union(lid_insert)


# -----------------------------
# Rear electronics pocket
# The wheel face is +Z. The pocket opens from the back side.
# -----------------------------
if include_electronics_pocket:
    print("Cutting electronics pocket...")
    pocket_cut = (
        cq.Workplane("XY")
        .rect(pocket_width, pocket_height)
        .vertices()
        .circle(pocket_corner_radius)
        .extrude(pocket_depth)
        .translate((0, 0, -0.1))
    )

    result = result.cut(pocket_cut)


# -----------------------------
# Shallow front inset showing electronics pocket footprint
# This is useful for a no-support hand-feel prototype.
# -----------------------------
if include_pocket_footprint_inset:
    print("Cutting pocket footprint inset...")
    result = (
        result.faces(">Z")
        .workplane()
        .rect(pocket_width, pocket_height)
        .cutBlind(-pocket_footprint_inset_depth)
    )


# -----------------------------
# Wire/sensor access hole through the hub
# -----------------------------
if include_center_hole:
    print("Cutting center hole...")
    result = result.faces(">Z").workplane().hole(wire_hole_diameter)


# -----------------------------
# Small mounting holes inside electronics pocket
# -----------------------------
if include_mounting_holes:
    print("Cutting mounting holes...")
    mount_points = [
        (-mount_hole_spacing_x / 2, -mount_hole_spacing_y / 2),
        (mount_hole_spacing_x / 2, -mount_hole_spacing_y / 2),
        (-mount_hole_spacing_x / 2, mount_hole_spacing_y / 2),
        (mount_hole_spacing_x / 2, mount_hole_spacing_y / 2),
    ]

    for x, y in mount_points:
        result = result.faces("<Z").workplane().center(x, y).hole(
            mount_hole_diameter,
            mount_hole_depth,
        )


# -----------------------------
# Preview and export
# -----------------------------
try:
    show_object(result)
    if cartridge_result is not None:
        show_object(cartridge_result.translate((hub_diameter, 0, 0)))
    if cartridge_lid_result is not None:
        show_object(cartridge_lid_result.translate((hub_diameter * 2.0, 0, 0)))
    running_in_cq_editor = True
except NameError:
    running_in_cq_editor = False

print(f"Exporting {step_filename}...")
exporters.export(result, step_filename)
print(f"Exporting {stl_filename}...")
exporters.export(result, stl_filename)

if cartridge_result is not None:
    print(f"Exporting {cartridge_step_filename}...")
    exporters.export(cartridge_result, cartridge_step_filename)
    print(f"Exporting {cartridge_stl_filename}...")
    exporters.export(cartridge_result, cartridge_stl_filename)
if cartridge_lid_result is not None:
    print(f"Exporting {cartridge_lid_step_filename}...")
    exporters.export(cartridge_lid_result, cartridge_lid_step_filename)
    print(f"Exporting {cartridge_lid_stl_filename}...")
    exporters.export(cartridge_lid_result, cartridge_lid_stl_filename)

print("Done.")

# Work around a native Windows heap-corruption crash that can happen during
# CadQuery/OCP shutdown after exports have already completed successfully.
if not running_in_cq_editor:
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
