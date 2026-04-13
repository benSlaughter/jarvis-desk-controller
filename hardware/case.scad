// ============================================================================
// Jarvis Desk Controller Case
// Two-part snap-fit enclosure for Beetle ESP32-C6 + custom PCB + RJ-12 socket
// ============================================================================
// Parametric design — adjust variables below to tune fit.
// Print both bottom_case() and top_case() (flip top for printing).
// ============================================================================

/* [PCB & Component Dimensions] */
pcb_length       = 40;    // mm, X-axis (landscape: RJ-12 left, USB right)
pcb_width        = 25;    // mm, Y-axis
pcb_thickness    = 1.6;   // mm, FR4 board

beetle_length    = 25;    // mm (Beetle board height)
beetle_width     = 20.5;  // mm (Beetle board width)
beetle_comp_h    = 5;     // mm, component height above Beetle PCB
beetle_bottom_h  = 2;     // mm, USB connector below Beetle PCB
header_height    = 8;     // mm, pin-header clearance above main PCB

rj12_width       = 15;    // mm, socket body width
rj12_depth       = 13;    // mm, socket body depth
rj12_height      = 12;    // mm, socket body height

usb_width        = 9;     // mm, USB-C opening width (generous)
usb_height       = 3.5;   // mm, USB-C opening height

/* [Case Parameters] */
wall             = 1.5;   // mm, wall thickness
floor_h          = 1.5;   // mm, floor / ceiling thickness
tolerance        = 0.3;   // mm, snap-fit & PCB clearance
snap_depth       = 0.8;   // mm, snap-fit tab protrusion
snap_width       = 6;     // mm, snap-fit tab width
snap_height      = 2;     // mm, snap-fit tab height

post_dia         = 4;     // mm, PCB support post diameter
post_hole_dia    = 2;     // mm, screw pilot or friction-fit pin

vent_slot_w      = 1.5;   // mm, ventilation slot width
vent_slot_l      = 8;     // mm, ventilation slot length
vent_spacing     = 3.5;   // mm, center-to-center vent slot spacing

/* [Derived Dimensions] */
// Internal cavity must fit everything with clearance
cavity_x  = pcb_length + 2 * tolerance;
cavity_y  = pcb_width  + 2 * tolerance;

// Heights: bottom holds PCB + RJ-12; top covers Beetle + headers
// Bottom interior: PCB sits on posts; tallest item below split is RJ-12 socket
pcb_post_h       = 2;     // mm, posts raise PCB off floor for bottom traces
bottom_interior  = pcb_post_h + pcb_thickness + rj12_height + tolerance;
top_interior     = header_height + beetle_comp_h + tolerance;

// Overall outside dimensions
outer_x = cavity_x + 2 * wall;
outer_y = cavity_y + 2 * wall;
bottom_h = floor_h + bottom_interior;
top_h    = floor_h + top_interior;

// Split line is at the top of the bottom shell
split_z  = bottom_h;

// Lip that the top slides over for alignment
lip_h    = 2;
lip_wall = 1;

$fn = 40;

// ============================================================================
// Modules
// ============================================================================

// --- Bottom Case ---
module bottom_case() {
    difference() {
        union() {
            // Outer shell
            difference() {
                rounded_box(outer_x, outer_y, bottom_h, r=2);
                // Hollow interior
                translate([wall, wall, floor_h])
                    cube([cavity_x, cavity_y, bottom_interior + 1]);
            }

            // Inner lip for top alignment
            translate([wall - lip_wall, wall - lip_wall, bottom_h])
                difference() {
                    cube([cavity_x + 2*lip_wall, cavity_y + 2*lip_wall, lip_h]);
                    translate([lip_wall + tolerance/2, lip_wall + tolerance/2, -0.1])
                        cube([cavity_x - tolerance, cavity_y - tolerance, lip_h + 0.2]);
                }

            // PCB support posts (4 corners of PCB area)
            pcb_posts();

            // Snap-fit catches on long sides
            bottom_snaps();
        }

        // RJ-12 opening — on the +X edge (one short/25mm edge of PCB)
        rj12_cutout();

        // USB-C opening — on the -X edge (opposite short edge)
        usb_cutout();

        // Bottom ventilation slots
        bottom_vents();
    }
}

// --- Top Case ---
module top_case() {
    difference() {
        union() {
            // Outer shell (printed upside-down, modeled right-side-up)
            difference() {
                translate([0, 0, split_z])
                    rounded_box(outer_x, outer_y, top_h, r=2);
                // Hollow interior
                translate([wall, wall, split_z - 0.1])
                    cube([cavity_x, cavity_y, top_interior + 0.1]);
            }

            // Inner ribs to press PCB down onto posts (optional hold-down)
            top_pcb_hold_downs();
        }

        // Slot for bottom lip
        translate([wall - lip_wall - tolerance/2, wall - lip_wall - tolerance/2, split_z - 0.1])
            cube([cavity_x + 2*lip_wall + tolerance, cavity_y + 2*lip_wall + tolerance, lip_h + 0.2]);

        // Snap-fit recesses on long sides
        top_snap_recesses();

        // RJ-12 opening (extends into top too)
        rj12_cutout_top();

        // USB-C opening (extends into top)
        usb_cutout_top();

        // Top ventilation slots
        top_vents();
    }
}

// ============================================================================
// Helper Modules
// ============================================================================

module rounded_box(x, y, z, r=2) {
    // Box with rounded vertical edges
    hull() {
        for (dx = [r, x-r], dy = [r, y-r])
            translate([dx, dy, 0])
                cylinder(r=r, h=z);
    }
}

// PCB support posts — near the 4 corners of the PCB footprint
module pcb_posts() {
    inset = 3;  // mm from PCB edge to post center
    positions = [
        [wall + tolerance + inset,                wall + tolerance + inset],
        [wall + tolerance + pcb_length - inset,   wall + tolerance + inset],
        [wall + tolerance + inset,                wall + tolerance + pcb_width - inset],
        [wall + tolerance + pcb_length - inset,   wall + tolerance + pcb_width - inset],
    ];
    for (p = positions) {
        translate([p[0], p[1], floor_h])
            difference() {
                cylinder(d=post_dia, h=pcb_post_h);
                // Pilot hole for alignment pin
                translate([0, 0, -0.1])
                    cylinder(d=post_hole_dia, h=pcb_post_h + 0.2);
            }
    }
}

// Hold-down nubs on the top shell interior — keep PCB from rattling
module top_pcb_hold_downs() {
    inset = 3;
    nub_h = 1.5;
    nub_d = 3;
    // Z position: ceiling minus nub height, pressing onto Beetle top
    z_top = split_z + top_interior - nub_h + floor_h;
    positions = [
        [wall + tolerance + pcb_length/2 - 6, wall + tolerance + inset],
        [wall + tolerance + pcb_length/2 + 6, wall + tolerance + inset],
        [wall + tolerance + pcb_length/2 - 6, wall + tolerance + pcb_width - inset],
        [wall + tolerance + pcb_length/2 + 6, wall + tolerance + pcb_width - inset],
    ];
    for (p = positions)
        translate([p[0], p[1], split_z])
            cylinder(d=nub_d, h=top_interior - 1);
}

// RJ-12 cutout on -X face (left wall)
module rj12_cutout() {
    cy = (outer_y - rj12_width) / 2;
    cz = floor_h + pcb_post_h + pcb_thickness - 1;
    translate([-0.1, cy, cz])
        cube([wall + 0.2, rj12_width, rj12_height + 2]);
}

// RJ-12 cutout extending into top shell
module rj12_cutout_top() {
    cy = (outer_y - rj12_width) / 2;
    cz = split_z - 0.1;
    translate([-0.1, cy, cz])
        cube([wall + 0.2, rj12_width, top_h + 0.2]);
}

// USB-C cutout on +X face (right wall)
module usb_cutout() {
    cx = outer_x - wall - 0.1;
    cy = (outer_y - usb_width) / 2;
    cz = floor_h + pcb_post_h + pcb_thickness + header_height - beetle_bottom_h - usb_height/2;
    translate([cx, cy, cz])
        cube([wall + 0.2, usb_width, usb_height + tolerance]);
}

// USB-C cutout extending into top shell
module usb_cutout_top() {
    cx = outer_x - wall - 0.1;
    cy = (outer_y - usb_width) / 2;
    cz = split_z - 0.1;
    translate([cx, cy, cz])
        cube([wall + 0.2, usb_width, usb_height + tolerance + 2]);
}

// Snap-fit tabs on bottom case (along Y walls)
module bottom_snaps() {
    // Two snaps on each long side (+Y and -Y walls)
    for (side = [0, 1]) {
        for (i = [0, 1]) {
            sx = outer_x * (0.3 + 0.4 * i) - snap_width/2;
            sy = side == 0 ? -snap_depth : outer_y;
            sz = bottom_h - snap_height - 1;
            translate([sx, sy, sz])
                snap_tab();
        }
    }
}

module snap_tab() {
    // Small ramped tab
    hull() {
        cube([snap_width, 0.1, snap_height]);
        translate([0, snap_depth, snap_height * 0.6])
            cube([snap_width, 0.1, snap_height * 0.4]);
    }
}

// Matching recesses in top case for snaps
module top_snap_recesses() {
    for (side = [0, 1]) {
        for (i = [0, 1]) {
            sx = outer_x * (0.3 + 0.4 * i) - snap_width/2 - tolerance/2;
            sy = side == 0 ? -0.1 : outer_y - wall - snap_depth - tolerance;
            sz = split_z - snap_height - 1 - tolerance/2;
            translate([sx, sy, sz])
                cube([snap_width + tolerance, wall + snap_depth + tolerance + 0.2,
                      snap_height + tolerance]);
        }
    }
}

// Ventilation slots on bottom shell (on top surface, which faces up)
module bottom_vents() {
    // Slots along the long axis on the floor (for airflow underneath is less
    // useful, so we put them on the two long-side walls instead)
    num_slots = floor((outer_y - 8) / vent_spacing);
    start_y = (outer_y - (num_slots - 1) * vent_spacing) / 2;
    for (side = [0, 1]) {
        sx = side == 0 ? -0.1 : outer_x - wall - 0.1;
        for (i = [0 : num_slots - 1]) {
            translate([sx, start_y + i * vent_spacing - vent_slot_w/2,
                       floor_h + pcb_post_h + 2])
                cube([wall + 0.2, vent_slot_w, vent_slot_l]);
        }
    }
}

// Ventilation slots on top shell (side walls + ceiling)
module top_vents() {
    // Side wall vents
    num_slots = floor((outer_y - 8) / vent_spacing);
    start_y = (outer_y - (num_slots - 1) * vent_spacing) / 2;
    for (side = [0, 1]) {
        sx = side == 0 ? -0.1 : outer_x - wall - 0.1;
        for (i = [0 : num_slots - 1]) {
            translate([sx, start_y + i * vent_spacing - vent_slot_w/2,
                       split_z + 2])
                cube([wall + 0.2, vent_slot_w, vent_slot_l]);
        }
    }
    // Ceiling vents (grid of slots)
    ceil_z = split_z + top_h - floor_h - 0.1;
    num_x = 3;
    num_y = 2;
    x_spacing = (outer_x - 12) / (num_x - 1);
    y_spacing = (outer_y - 10) / max(num_y - 1, 1);
    for (ix = [0 : num_x - 1]) {
        for (iy = [0 : num_y - 1]) {
            translate([6 + ix * x_spacing - vent_slot_l/2,
                       5 + iy * y_spacing - vent_slot_w/2,
                       ceil_z])
                cube([vent_slot_l, vent_slot_w, floor_h + 0.2]);
        }
    }
}

// ============================================================================
// Render — uncomment what you need, or use OpenSCAD Customizer
// ============================================================================

// Show both parts side by side for preview
// To export STL: render one at a time.

// Bottom case
bottom_case();

// Top case, offset to the right for visibility
translate([outer_x + 10, 0, 0])
    top_case();

// Uncomment below to show assembled (top placed on bottom):
// color("SteelBlue", 0.6) top_case();
