import os
import sys

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

# Modular hub with removable electronics cartridge
include_modular_cartridge_hub = True
hub_rear_lip_thickness = 3.0
cartridge_cavity_diameter = 62.4
cartridge_key_flat_depth = 6.0
cartridge_key_angle = -90.0
cartridge_floor_thickness = 2.4

# -----------------------------
# Derived values
# -----------------------------
outer_radius = wheel_outer_diameter / 2
inner_radius = wheel_inner_diameter / 2
hub_radius = hub_diameter / 2

spoke_length = inner_radius - hub_radius + 8.0
spoke_center_offset = hub_radius + spoke_length / 2 - 4.0
cartridge_cavity_depth = hub_thickness - hub_rear_lip_thickness


def add_rear_charge_port(
    cartridge,
    width,
    height,
    center_x,
    center_y,
    center_z,
    floor_thickness,
):
    """Cut a rear-facing USB plug opening through the cartridge back cap."""
    port_cut = (
        cq.Workplane("XY")
        .box(width, height, floor_thickness + 0.6)
        .translate((center_x, center_y, center_z + (floor_thickness / 2.0)))
    )
    return cartridge.cut(port_cut)


def make_cylinder_hole(
    diameter,
    depth,
    center_x,
    center_y,
    center_z
):
    """Cut a rear-facing hole"""
    return (
        cq.Workplane("XY")
            .circle(diameter / 2.0)
            .extrude(-depth)
            .translate((center_x, center_y, center_z))
    )


def make_upright_board_slot(
    board_width,
    slot_depth,
    center_x,
    center_y,
    center_z,
    wall_thickness,
    wall_height,
    clearance,
):
    """Create side rails for a board standing orthogonal to the back cap."""
    rail_x = board_width / 2.0 + clearance + wall_thickness / 2.0
    left_rail = make_rail(center_x - rail_x, center_y, center_z, wall_thickness, wall_height, slot_depth)
    right_rail = make_rail(center_x + rail_x, center_y, center_z, wall_thickness, wall_height, slot_depth)
    return left_rail.union(right_rail)

def make_rail(
    center_x,
    center_y,   
    center_z,
    wall_thickness, 
    slot_depth, 
    wall_height
):
    rail_size = (wall_thickness, wall_height, slot_depth)

    return (
        cq.Workplane("XY")
        .box(*rail_size)
        .translate((center_x, center_y, center_z))
    )


def add_slot(
    cartridge,
    board_width,
    slot_depth,
    center_x,
    center_y,
    center_z,
    wall_thickness,
    wall_depth,
    clearance,
):
    """Add an upright charger-board slot aligned with the rear USB opening."""
    print("Adding upright charger slot...")
    return cartridge.union(
        make_upright_board_slot(
            board_width,
            slot_depth,
            center_x,
            center_y,
            center_z,
            wall_thickness,
            wall_depth,
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
    inset_height = 5.0

    screw_channel_diameter = 4
    screw_head_diameter = 6.5
    screw_head_height = 2.5

    channel_length = full_screw_length - inset_height - screw_head_height

    if channel_length < 0:
        raise ValueError("full_screw_length is too short for insert + screw head")

    # First how is for inset; and it starts at the Z location
    insert_center_z = z_start
    hole = make_cylinder_hole(
        inset_diameter,
        -inset_height,
        x,
        y,
        insert_center_z,
    )

    # Next add a channel for screw:
    channel_center_z = z_start + inset_height
    hole = hole.union(
        make_cylinder_hole(
            screw_channel_diameter,
            -channel_length,
            x,
            y,
            channel_center_z,
        )
    )

    # Now add long channel for screw
    hole = hole.union(
        make_cylinder_hole(
            screw_head_diameter,
            -(screw_head_height + screw_head_extra_depth),
            x,
            y,
            channel_center_z + channel_length,
        )
    )

    return hole


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

    wiring_spoke = (
        cq.Workplane("YZ")
        .circle(spoke_width / 7)
        .extrude(spoke_length * 3)
        .translate((0, 0, spoke_thickness / 2))
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

screw_count = 3
screw_length = 12

for i in range(screw_count):
    angle = i * (360.0 / screw_count)
    screw = make_screw_hole(spoke_center_offset + 25, 0, -12.5, screw_length).rotate((0, 0, 0), (1, 0, 0), 180).rotate((0, 0, 0), (0, 0, 1), angle - 45)
    result = result.cut(screw)
    #show_debug(screw)
    
# -----------------------------
# Modular cartridge hub
# The wheel gets a keyed cavity from the back.
# A separate starter cartridge shell is exported for iteration.
# -----------------------------
include_internal_sensor_housing = True

charge_port_width = 9.5
charge_port_height = 5.0
charge_board_slot_clearance = 0.6

if include_modular_cartridge_hub:
    # Create cylindrical cavity where we'll house electronics:
    cavity = (
        cq.Workplane("XY")
        .circle(cartridge_cavity_diameter / 2.0)
        .extrude(cartridge_cavity_depth)
        .translate((0, 0, -12 + hub_thickness - cartridge_cavity_depth))
    )
    result = result.cut(cavity)
    # show_debug(cavity)

    # Split in half for printing, (will use screw holes to hold together)
    first_half, second_half = cut_in_half_z(result, 11)

    if include_internal_sensor_housing:
        
        # Create battery container
        battery_container_top = make_spanning_tray(0, 29, 1, tray_width=36, tray_thickness=2, depth=20)
        battery_container_bottom = make_spanning_tray(4, 22, 1, tray_width=40, tray_thickness=2, depth=20)
        battery_container_rail_long = make_rail(14, 26, 1, wall_thickness=2, slot_depth=20, wall_height=6)
        battery_container_rail_short = make_rail(-15, 26, -6, wall_thickness=2, slot_depth=6, wall_height=6)
        first_half = first_half.union(battery_container_top).union(battery_container_bottom).union(battery_container_rail_long).union(battery_container_rail_short)

        # Battery management system board container
        bms_container_bottom = make_spanning_tray(12, 11, 1, tray_width=36, tray_thickness=2, depth=20)
        first_half = first_half.union(bms_container_bottom)
        first_half = add_slot(first_half, 19, 4, 6, 14, 1, 2, 20, charge_board_slot_clearance)

        # Battery management system board mounting pegs
        bms_charger_pegs = make_horizontal_board_pegs(
            center_x=6,
            center_y=15,
            center_z=0,
            spacing_x=14.0,
            spacing_z=15.0,
            peg_diameter=1.8,
            peg_length=4.0,
        )
        first_half = first_half.union(bms_charger_pegs)
        
        # Battery Charging port
        first_half = add_rear_charge_port(first_half, charge_port_width - 1, charge_port_height * .75, 6, 15, -10, cartridge_floor_thickness + 1)

        # On/off switch:
        first_half = add_rear_charge_port(first_half, charge_port_height * .80,  charge_port_width, 25, -1, -10, cartridge_floor_thickness + 1)
        on_off_top = make_spanning_tray(25, 7, -6, tray_width=14, tray_thickness=2, depth=7)
        on_off_rail_right = make_rail(30, -1, -6, wall_thickness=2, slot_depth=7, wall_height=15)
        on_off_rail_left = make_rail(20, -1, -6, wall_thickness=2, slot_depth=7, wall_height=15)
        on_off_bottom = make_spanning_tray(25, -8.5, -6, tray_width=14, tray_thickness=2, depth=7)
        first_half = first_half.union(on_off_top).union(on_off_rail_right).union(on_off_rail_left).union(on_off_bottom)

        # # BNO055 Sensor pegs
        # sensor_pegs = make_flat_board_pegs(
        #     center_x=0,
        #     center_y=-2,
        #     center_z=cartridge_floor_thickness - 11,
        #     spacing_x=21.2,
        #     spacing_y=15.0,
        #     peg_diameter=1.8,
        #     peg_length=4.0,
        # )
        # first_half = first_half.union(sensor_pegs)


        # XIAO MCU rear data port
        first_half = add_rear_charge_port(first_half, charge_port_width, charge_port_height * 0.6, 6, -18.5, -10, cartridge_floor_thickness + 1)
        
        # XIAO MCU holder tray
        mcu_tray = make_spanning_tray(8, -23, 1, tray_width=36, tray_thickness=2, depth=20)
        first_half = first_half.union(mcu_tray)


cutting_tool = (
    cq.Workplane("XY")
    .box(200, 200, 20)
).translate((0, 0, 9))
test_cut_center = (
    cq.Workplane("XY")
    .circle(hub_radius*.9)
    .extrude(hub_thickness)
).translate((0, 0, -10))
cutting_tool = cutting_tool.cut(test_cut_center)
show_debug(cutting_tool)

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
try:
    show_object(first_half, name="steering_whee_bottom")
    show_object(second_half.translate((wheel_outer_diameter + 10, 0, 0)), name="steering_whee_top")

    running_in_cq_editor = True
except NameError:
    running_in_cq_editor = False


exporters.export(first_half, "steering_wheel_back.stl")
exporters.export(second_half, "steering_wheel_front.stl")

print("Done.")

# Work around a native Windows heap-corruption crash that can happen during
# CadQuery/OCP shutdown after exports have already completed successfully.
if not running_in_cq_editor:
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
