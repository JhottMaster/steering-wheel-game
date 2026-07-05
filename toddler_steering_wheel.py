import os
import sys
import math

import cadquery as cq
from cadquery import exporters


# -----------------------------
# Main dimensions
# -----------------------------
wheel_outer_diameter = 184.0
wheel_inner_diameter = 118.0
wheel_thickness = 18.0
grip_roundover_radius = 7.0

hub_diameter = 78.0
hub_thickness = 35.0
hub_roundover_radius = 2.0

spoke_count = 3
spoke_width = 20.0
spoke_thickness = 14.0
spoke_roundover_radius = 2.0

# Split hub electronics cavity
hub_rear_lip_thickness = 3.0
cartridge_cavity_diameter = 62.4
port_cut_depth = 2.4

# -----------------------------
# Derived values
# -----------------------------
outer_radius = wheel_outer_diameter / 2
inner_radius = wheel_inner_diameter / 2
hub_radius = hub_diameter / 2

spoke_length = inner_radius - hub_radius + 8.0
spoke_center_offset = hub_radius + spoke_length / 2 - 4.0
cartridge_cavity_depth = hub_thickness - hub_rear_lip_thickness


def cut_rectangular_port(
    model,
    width,
    height,
    center_x,
    center_y,
    center_z,
    floor_thickness,
):
    """Cut a rectangular access opening through a wall or floor."""
    port_cut = (
        cq.Workplane("XY")
        .box(width, height, floor_thickness + 0.6)
        .translate((center_x, center_y, center_z + (floor_thickness / 2.0)))
    )
    return model.cut(port_cut)


def make_cylinder_hole(
    diameter,
    length,
    center_x,
    center_y,
    start_z,
    z_direction=-1,
):
    """Create a vertical cylindrical cut body from start_z."""
    if length < 0:
        raise ValueError("length must be positive")

    return (
        cq.Workplane("XY")
            .circle(diameter / 2.0)
            .extrude(length * z_direction)
            .translate((center_x, center_y, start_z))
    )


def make_upright_board_slot(
    board_width,
    slot_depth,
    center_x,
    center_y,
    center_z,
    rail_thickness,
    rail_height,
    clearance,
):
    """Create side rails for a board standing orthogonal to the back cap."""
    rail_x = board_width / 2.0 + clearance + rail_thickness / 2.0
    left_rail = make_rail(
        center_x - rail_x,
        center_y,
        center_z,
        size_x=rail_thickness,
        size_y=slot_depth,
        size_z=rail_height,
    )
    right_rail = make_rail(
        center_x + rail_x,
        center_y,
        center_z,
        size_x=rail_thickness,
        size_y=slot_depth,
        size_z=rail_height,
    )
    return left_rail.union(right_rail)

def make_rail(
    center_x,
    center_y,   
    center_z,
    size_x,
    size_y,
    size_z,
):
    return (
        cq.Workplane("XY")
        .box(size_x, size_y, size_z)
        .translate((center_x, center_y, center_z))
    )


def add_slot(
    model,
    board_width,
    slot_depth,
    center_x,
    center_y,
    center_z,
    rail_thickness,
    rail_height,
    clearance,
):
    """Add an upright board slot."""
    print("Adding upright board slot...")
    return model.union(
        make_upright_board_slot(
            board_width,
            slot_depth,
            center_x,
            center_y,
            center_z,
            rail_thickness,
            rail_height,
            clearance,
        )
    )

def make_flat_board_pegs(
    center_x,
    center_y,
    center_z,
    spacing_x,
    spacing_y,
    peg_diameter,
    peg_length,
):
    pegs = cq.Workplane("XY")

    points = [
        (-spacing_x / 2.0, -spacing_y / 2.0),
        (spacing_x / 2.0, -spacing_y / 2.0),
        (-spacing_x / 2.0, spacing_y / 2.0),
        (spacing_x / 2.0, spacing_y / 2.0),
    ]
    for offset_x, offset_y in points:
        peg = (
            cq.Workplane("XY")
            .circle(peg_diameter / 2.0)
            .extrude(peg_length)
            .translate((
                center_x + offset_x,
                center_y + offset_y,
                center_z,
            ))
        )

        pegs = pegs.union(peg)

    return pegs

def make_horizontal_board_pegs(
    center_x,
    center_y,
    center_z,
    spacing_x,
    spacing_z,
    peg_diameter,
    peg_length,
):
    pegs = cq.Workplane("XY")

    points = [
        (-spacing_x / 2.0, -spacing_z / 2.0),
        (spacing_x / 2.0, -spacing_z / 2.0),
        (-spacing_x / 2.0, spacing_z / 2.0),
        (spacing_x / 2.0, spacing_z / 2.0),
    ]
    for offset_x, offset_z in points:
        peg = (
            cq.Workplane("XY")
            .circle(peg_diameter / 2.0)
            .extrude(peg_length)
            # Cylinder starts along Z; rotate it so length runs along Y.
            .rotate((0, 0, 0), (1, 0, 0), 90)
            .translate((
                center_x + offset_x,
                center_y,
                center_z + offset_z,
            ))
        )

        pegs = pegs.union(peg)

    return pegs

def make_spanning_tray(
    center_x,
    center_y,
    center_z,
    tray_width,
    tray_thickness,
    depth
):
    """Create a tray for mounting components"""
    tray_outer = (
        cq.Workplane("XY")
        .box(tray_width, tray_thickness, depth)
        .translate((center_x, center_y, center_z))
    )

    return tray_outer

def cut_in_half_z(model, z_cut_location):
    cutter = cq.Workplane("XY").box(1000, 1000, 1000)
    lower_half = model.cut(cutter.translate((0, 0, z_cut_location + 500)))
    upper_half = model.cut(cutter.translate((0, 0, z_cut_location - 500)))
    return lower_half, upper_half


def show_debug(object, name="debug_obj"):
    try:
        show_object(
            object,
            name=name,
            options={
                "color": (255, 0, 0),
                "alpha": 0.25,
            },
        )
    except NameError:
        pass

def make_screw_hole(x, y, z_start, full_screw_length, screw_head_extra_depth = 10):
    """Create a stepped vertical screw/inset cutout starting at z_start."""
    inset_diameter = 4.5
    inset_height = 8.0

    screw_channel_diameter = 4
    screw_head_diameter = 6.5
    screw_head_height = 2.5

    channel_length = full_screw_length - inset_height - screw_head_height

    if channel_length < 0:
        raise ValueError("full_screw_length is too short for insert + screw head")

    # First hole is the inset/recess for the brass heat-set insert.
    inset_start_z = z_start
    hole = make_cylinder_hole(
        inset_diameter,
        inset_height,
        x,
        y,
        inset_start_z,
        z_direction=1,
    )

    # Next add a channel for screw:
    channel_start_z = z_start + inset_height
    hole = hole.union(
        make_cylinder_hole(
            screw_channel_diameter,
            channel_length,
            x,
            y,
            channel_start_z,
            z_direction=1,
        )
    )

    # Now add long channel for screw
    hole = hole.union(
        make_cylinder_hole(
            screw_head_diameter,
            screw_head_height + screw_head_extra_depth,
            x,
            y,
            channel_start_z + channel_length,
            z_direction=1,
        )
    )

    return hole


def make_half_ellipse_profile(
    half_width,
    height,
    segments=32,
    flat_top_ratio=0.85,
):
    """Create a rounded wire-channel profile with a shallow flat top."""
    if half_width <= 0 or height <= 0:
        raise ValueError("half_width and height must be positive")
    if segments < 3:
        raise ValueError("segments must be at least 3")
    if not 0 < flat_top_ratio < 1:
        raise ValueError("flat_top_ratio must be between 0 and 1")

    points = []
    top_cut_z = height * flat_top_ratio
    start_angle = math.pi - math.asin(flat_top_ratio)
    end_angle = (2.0 * math.pi) + math.asin(flat_top_ratio)
    z_scale = height / (height + top_cut_z)

    for i in range(segments + 1):
        t = start_angle + ((end_angle - start_angle) * i / segments)
        y = math.cos(t) * half_width
        raw_z = math.sin(t) * height
        # Shift the clipped top to z=0, then scale so the bottom remains at -height.
        z = (raw_z - top_cut_z) * z_scale
        points.append((y, z))

    profile = cq.Workplane("YZ").polyline(points)
    profile = profile.lineTo(points[0][0], points[0][1]).close()
    return profile

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
).translate((0, 0, -10))


# -----------------------------
# Spokes
# -----------------------------
print("Building spokes...")
spokes = cq.Workplane("XY")

wheel_grip_width = wheel_outer_diameter - wheel_inner_diameter
button_hold_diameter = 16
button_insets = []
for i in range(spoke_count):
    angle = i * (360.0 / spoke_count)

    spoke = (
        cq.Workplane("XY")
        .box(spoke_length, spoke_width, spoke_thickness)
        .edges("|X")
        .fillet(spoke_roundover_radius)
        .translate((spoke_center_offset, 0, spoke_thickness / 2))
        .rotate((0, 0, 0), (0, 0, 1), angle - 90)
    )
    spokes = spokes.union(spoke)

    # tunnels for button wires
    wiring_spoke = (
        make_half_ellipse_profile(
            half_width=4.0,
            height=6.0,
            flat_top_ratio=0.95,
        )
        .extrude(spoke_length * 3)
        .translate((0, 0, 11))
        .rotate((0, 0, 0), (0, 0, 1), angle - 90)
    )

    if i == 1 or i == 2:
        inset_offset = spoke_center_offset + spoke_width + ((wheel_grip_width/2) * .2)
        button_inset_hole = make_cylinder_hole(button_hold_diameter, wheel_thickness*.5, inset_offset, 0, 20).rotate((0, 0, 0), (0, 0, 1), angle - 90)
        # show_debug(button_inset_hole)
        hex_prism = (
            cq.Workplane("XY")
            .polygon(6, 22.5)
            .extrude(wheel_thickness*.5)
        ).rotate((0, 0, 0), (0, 0, 1), angle - 90).translate((inset_offset, 0, wheel_thickness-16)).rotate((0, 0, 0), (0, 0, 1), angle - 90)
        # show_debug(hex_prism)
        wiring_spoke = wiring_spoke.union(button_inset_hole).union(hex_prism)
    
    button_insets.append(wiring_spoke)

# -----------------------------
# Combine main body
# -----------------------------
print("Combining body...")
result = rim.union(spokes).union(hub)
for button_inset in button_insets:
    result = result.cut(button_inset)

# Make antenna compartment:
antenna_compartment = cq.Workplane("XY").box(41, 22, 8).edges().fillet(1).translate((0, -spoke_center_offset-25, 7))
# show_debug(antenna_compartment)
result = result.cut(antenna_compartment)

# Add screw holes
screw_count = 4
screw_length = 12

for i in range(screw_count):
    angle = i * (360.0 / screw_count)
    screw = make_screw_hole(spoke_center_offset + 25, 0, -15.5, screw_length).rotate((0, 0, 0), (1, 0, 0), 180).rotate((0, 0, 0), (0, 0, 1), angle - 45)
    result = result.cut(screw)
    # show_debug(screw)
    
# -----------------------------
# Split hub electronics cavity
# The wheel gets a cylindrical electronics cavity, then the whole body is split
# into printable front/back halves.
# -----------------------------
charge_port_width = 9.5
charge_port_height = 5.0
charge_board_slot_clearance = 0.6

# Create cylindrical cavity where we'll house electronics.
cavity = (
    cq.Workplane("XY")
    .circle(cartridge_cavity_diameter / 2.0)
    .extrude(cartridge_cavity_depth)
    .translate((0, 0, -12 + hub_thickness - cartridge_cavity_depth))
)
result = result.cut(cavity)
# show_debug(cavity)

# Split in half for printing, using screw holes to hold the halves together.
steering_wheel_back, steering_wheel_front = cut_in_half_z(result, 11)


############## Steering Wheel Front status LED

led_socket_outer_diameter = 8.5
led_socket_inner_diameter = 6.5
led_socket_height = 10.0
led_socket_floor_thickness = 4

led_socket_center_x = 0
led_socket_center_y = 5
led_socket_base_z = hub_thickness - led_socket_height - 10

led_lead_spacing = 2.54
led_lead_hole_diameter = 1.5
led_lead_hole_depth = 5.0


led_light_cutout = (
    cq.Workplane("XY")
    .circle(5 / 2.0)
    .extrude(10)
    .translate((led_socket_center_x, led_socket_center_y, 20))
)
# show_debug(led_light_cutout, name="led_light_cutout")
steering_wheel_front = steering_wheel_front.cut(led_light_cutout)

# Created the hole on top of steering wheen for front LED.
led_socket_outer = (
    cq.Workplane("XY")
    .circle(led_socket_outer_diameter / 2.0)
    .extrude(hub_thickness - 3)
    .translate((led_socket_center_x, led_socket_center_y, -10))
)
wire_slot = make_rail(0, led_socket_center_y, 3, size_x=led_socket_outer_diameter, size_y=3.5, size_z=25)
led_socket_outer =led_socket_outer.cut(wire_slot)

led_body_cavity = make_cylinder_hole(
    led_socket_inner_diameter,
    led_socket_height - led_socket_floor_thickness + 0.2,
    led_socket_center_x,
    led_socket_center_y,
    led_socket_base_z + led_socket_height + 0.1,
)

led_socket = led_socket_outer.cut(led_body_cavity)

for led_lead_offset_x in (-led_lead_spacing / 2.0, led_lead_spacing / 2.0):
    led_lead_hole = make_cylinder_hole(
        led_lead_hole_diameter,
        led_lead_hole_depth,
        led_socket_center_x + led_lead_offset_x,
        led_socket_center_y,
        led_socket_base_z + led_socket_floor_thickness + 0.1,
    )
    led_socket = led_socket.cut(led_lead_hole)

led_socket_rail_bridge = (
    cq.Workplane("ZY")
    .circle(3.5 / 2.0)
    .extrude(led_socket_outer_diameter)
    .translate((led_socket_outer_diameter/2.0, led_socket_center_y, led_socket_base_z))
)
led_socket = led_socket.cut(led_socket_rail_bridge)
# show_debug(led_socket_rail_bridge, name="led_socket_rail_bridge")

# show_debug(led_socket, name="led_socket")
exporters.export(led_socket, "led_socket.stl")

# steering_wheel_front = steering_wheel_front.union(led_socket)
steering_wheel_back = steering_wheel_back.union(led_socket)


############## Steering Wheel Back Electronic compartments

# Create battery container
battery_container_top = make_spanning_tray(0, 28.5, 1, tray_width=36, tray_thickness=2, depth=20)
battery_container_bottom = make_spanning_tray(4, 22, 1, tray_width=38, tray_thickness=2, depth=20)
battery_container_rail_long = make_rail(14, 26, 1, size_x=2, size_y=6, size_z=20)
battery_container_rail_short = make_rail(-15, 26, -6, size_x=2, size_y=6, size_z=6)
steering_wheel_back = steering_wheel_back.union(battery_container_top).union(battery_container_bottom).union(battery_container_rail_long).union(battery_container_rail_short)

# Battery management system board container
bms_container_bottom = make_spanning_tray(11.5, 12.5, 1, tray_width=35, tray_thickness=2, depth=20)
button_wire_channel = cq.Workplane("XY").circle(5).extrude(4).rotate((1, 0, 0), (0,0,0), 90).translate((24, 9.5, 8))
bms_container_bottom = bms_container_bottom.cut(button_wire_channel)
#show_debug(button_wire_channel)
steering_wheel_back = steering_wheel_back.union(bms_container_bottom)
steering_wheel_back = add_slot(
    steering_wheel_back,
    board_width=19,
    slot_depth=4,
    center_x=6,
    center_y=14.5,
    center_z=1,
    rail_thickness=2,
    rail_height=20,
    clearance=charge_board_slot_clearance,
)

# Front-half BMS retainer
# This adds a simple inward boss on the cylindrical wall so the front half can
# brace the back side of the BMS area when the shell is assembled.
bms_front_retainer = make_spanning_tray(
    -3,
    20.5,
    18.75,
    tray_width=5,
    tray_thickness=18,
    depth=12,
)
bms_front_retainer2 = make_spanning_tray(
    15,
    20,
    18.75,
    tray_width=5,
    tray_thickness=18,
    depth=12,
)
bms_retainers = bms_front_retainer.union(bms_front_retainer2)
# show_debug(bms_retainers, name="bms_retainers")
steering_wheel_front = steering_wheel_front.union(bms_retainers)

# Battery Charging port
steering_wheel_back = cut_rectangular_port(steering_wheel_back, charge_port_width - 1, charge_port_height * .75, 6, 16.5, -10, port_cut_depth + 1)

# On/off switch:
steering_wheel_back = cut_rectangular_port(steering_wheel_back, charge_port_height * .80, charge_port_width * .9, 25, 0, -10, port_cut_depth + 1)
on_off_top = make_spanning_tray(25, 7, -6, tray_width=14, tray_thickness=2, depth=7)
on_off_rail_right = make_rail(29.5, -1, -6, size_x=2, size_y=15, size_z=7)
on_off_rail_left = make_rail(20.5, -1, -6, size_x=2, size_y=15, size_z=7)
on_off_bottom = make_spanning_tray(25, -8, -6, tray_width=14, tray_thickness=2, depth=7)
steering_wheel_back = steering_wheel_back.union(on_off_top).union(on_off_rail_right).union(on_off_rail_left).union(on_off_bottom)

# BNO055 sensor board container
bms_container_bottom = make_spanning_tray(4, -8, 1, tray_width=36, tray_thickness=2, depth=20)
steering_wheel_back = steering_wheel_back.union(bms_container_bottom)
steering_wheel_back = add_slot(
    steering_wheel_back,
    board_width=26,
    slot_depth=4,
    center_x=4,
    center_y=-6,
    center_z=1,
    rail_thickness=2,
    rail_height=20,
    clearance=0.5,
)
bms_container_lip_1 = make_spanning_tray(-9.5, -3.5, 1, tray_width=4, tray_thickness=2, depth=20)
bms_container_lip_2 = make_spanning_tray(17.5, -3.5, 1, tray_width=4, tray_thickness=2, depth=20)
steering_wheel_back = steering_wheel_back.union(bms_container_lip_1).union(bms_container_lip_2)
# BNO055 sensor pegs
# sensor_pegs = make_flat_board_pegs(
#     center_x=0,
#     center_y=-2,
#     center_z=port_cut_depth - 12,
#     spacing_x=21.2,
#     spacing_y=15.0,
#     peg_diameter=1.8,
#     peg_length=4.0,
# )
# steering_wheel_back = steering_wheel_back.union(sensor_pegs)


# XIAO MCU rear data port
steering_wheel_back = cut_rectangular_port(steering_wheel_back, charge_port_width, charge_port_height * 0.6, 6, -18.5, -10, port_cut_depth + 1)

# XIAO MCU holder tray with wire channel
mcu_tray = make_spanning_tray(8, -23, 1, tray_width=36, tray_thickness=2, depth=20)
mcu_wire_channel = cq.Workplane("XY").circle(3).extrude(4).rotate((1, 0, 0), (0,0,0), 90).translate((1, -25, 0))
mcu_tray = mcu_tray.cut(mcu_wire_channel)
steering_wheel_back = steering_wheel_back.union(mcu_tray)
steering_wheel_back = add_slot(
    steering_wheel_back,
    board_width=17,
    slot_depth=4,
    center_x=6,
    center_y=-21,
    center_z=1,
    rail_thickness=2,
    rail_height=20,
    clearance=1,
)


cutting_tool = (
    cq.Workplane("XY")
    .box(400, 400, 20)
).translate((0, 0, 9))

test_cut_center = (
    cq.Workplane("XY")
    .circle(hub_radius*.9)
    .extrude(hub_thickness)
).translate((0, 0, -10))
cutting_tool = cutting_tool.cut(test_cut_center)

distance = 55
four_boxes = (
    cq.Workplane("XY")
    .rect(distance, distance)
    .vertices()
    .box(10, 10, 20)
).translate((0, 0, 9))
cutting_tool = cutting_tool.union(four_boxes)
# show_debug(cutting_tool, name="cutting tool")

smaller_cutting_tool = cutting_tool.val().scale(0.995)
# show_debug(smaller_cutting_tool, name="cutting tool smaller")

locking_holes = cq.Workplane("XY")
locking_hole_count = 2
for i in range(locking_hole_count):
    angle = i * (360.0 / locking_hole_count)
    lock_spoke = (
        cq.Workplane("YZ")
            .circle(spoke_width / 7)
            .extrude(7)
            .translate((30, 0, spoke_thickness / 2 - 2))
            .rotate((0, 0, 0), (0, 0, 1), angle)
    )
    locking_holes = locking_holes.union(lock_spoke)

# show_debug(locking_holes)
steering_wheel_back = steering_wheel_back.cut(locking_holes)

center_hub = steering_wheel_back.cut(smaller_cutting_tool)
steering_wheel_back = steering_wheel_back.intersect(cutting_tool)

# For printing quick prototypes testing screw fit
# test_screw_block = (
#     cq.Workplane("XY")
#     .box(10, 10, 16)
#     .translate((0, 0, 6))
# )
# test_screw_block = test_screw_block.cut(make_screw_hole(0, 0, 0, screw_length)).translate((0, 125, 0))
# show_debug(test_screw_block)
# exporters.export(test_screw_block, "test_screw_block.stl")

# -----------------------------
# Preview and export
# -----------------------------
offset_center_hub = 0 # -wheel_outer_diameter + 40
offset_wheel_front = 0 # wheel_outer_diameter + 10
try:
    show_object(center_hub.translate((offset_center_hub, 0, 0)), name="center_hub")
    show_object(steering_wheel_back, name="steering_wheel_back")
    show_object(steering_wheel_front.translate((offset_wheel_front, 0, 0)), name="steering_wheel_front")

    running_in_cq_editor = True
except NameError:
    running_in_cq_editor = False


exporters.export(steering_wheel_back, "steering_wheel_back.stl")
exporters.export(steering_wheel_front, "steering_wheel_front.stl")
exporters.export(center_hub, "center_hub.stl")

print("Done.")

# Work around a native Windows heap-corruption crash that can happen during
# CadQuery/OCP shutdown after exports have already completed successfully.
if not running_in_cq_editor:
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
