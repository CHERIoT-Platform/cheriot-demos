$fa=5;
$fs=0.1;

difference() {
    sphere(r=6);
    translate([0,0,-7]) {
        cube(14, center = true);
    }
    translate([0,0,5])
    cube([3.5,3.5,6], center=true);
}
