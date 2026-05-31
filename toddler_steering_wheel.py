import cadquery as cq
from cadquery import exporters


# -----------------------------
# Main dimensions
# -----------------------------
wheel_outer_diameter = 180.0
wheel_inner_diameter = 118.0
wheel_thickness = 16.0
grip_roundover_radius = 7.0

hub_diameter = 54.0
hub_thickness = 22.0
hub_roundover_radius = 2.0

spoke_count = 3
spoke_width = 20.0
spoke_thickness = 14.0
spoke_roundover_radius = 2.0

# Electronics pocket in back of hub
include_electronics_pocket = False
include_pocket_footprint_inset = True
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


# -----------------------------
# Derived values
# -----------------------------
outer_radius = wheel_outer_diameter / 2
inner_radius = wheel_inner_diameter / 2
hub_radius = hub_diameter / 2

spoke_length = inner_radius - hub_radius + 8.0
spoke_center_offset = hub_radius + spoke_length / 2 - 4.0


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
except NameError:
    pass

print(f"Exporting {step_filename}...")
exporters.export(result, step_filename)
print(f"Exporting {stl_filename}...")
exporters.export(result, stl_filename)
print("Done.")
