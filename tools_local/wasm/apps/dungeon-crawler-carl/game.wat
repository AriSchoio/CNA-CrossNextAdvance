(module
  (import "env" "gd_width" (func $gd_width (result i32)))
  (import "env" "gd_height" (func $gd_height (result i32)))
  (import "env" "gd_line_height" (func $gd_line_height (result i32)))
  (import "env" "gd_text" (func $gd_text (param i32 i32 i32 i32)))
  (import "env" "gd_rect" (func $gd_rect (param i32 i32 i32 i32 i32)))
  (import "env" "gd_log" (func $gd_log (param i32)))

  (memory (export "memory") 1)
  (data (i32.const 16) "DUNGEON CRAWLER\00")
  (data (i32.const 64) "Carl's Survival Run\00")
  (data (i32.const 128) "SURVIVE THE SYSTEM\00")
  (data (i32.const 192) "Confirm to enter the dungeon\00")
  (data (i32.const 256) "HP  | GOLD  | FLOOR\00")
  (data (i32.const 320) "CURRENT ENEMY\00")
  (data (i32.const 384) "CAVE GOBOBO\00")
  (data (i32.const 448) "ARMOR MANTIS\00")
  (data (i32.const 512) "SYSTEM WRAITH\00")
  (data (i32.const 576) "DUNGEON BOSS\00")
  (data (i32.const 640) "Up/Left/Right: move  Confirm: attack  Down: potion\00")
  (data (i32.const 768) "Potion used\00")
  (data (i32.const 832) "Victory! Press Confirm to descend\00")
  (data (i32.const 896) "RUN ENDED\00")
  (data (i32.const 960) "The dungeon claims another crawler\00")
  (data (i32.const 1024) "Confirm: retry\00")
  (data (i32.const 1088) "YOU REACHED FLOOR 10\00")
  (data (i32.const 1152) "Carl and Princess Fluff survive the dungeon!\00")

  (global $mode (mut i32) (i32.const 0))
  (global $floor (mut i32) (i32.const 1))
  (global $room (mut i32) (i32.const 1))
  (global $hp (mut i32) (i32.const 100))
  (global $maxhp (mut i32) (i32.const 100))
  (global $gold (mut i32) (i32.const 0))
  (global $xp (mut i32) (i32.const 0))
  (global $attack (mut i32) (i32.const 14))
  (global $potions (mut i32) (i32.const 3))
  (global $wins (mut i32) (i32.const 0))
  (global $enemyhp (mut i32) (i32.const 40))
  (global $enemymax (mut i32) (i32.const 40))

  (func $title
    (call $gd_rect (i32.const 12) (i32.const 12) (call $gd_width) (i32.const 72) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 26) (i32.const 16) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 58) (i32.const 64) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 112) (i32.const 128) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 148) (i32.const 192) (i32.const 0))
    (call $gd_rect (i32.const 24) (i32.const 190) (i32.sub (call $gd_width) (i32.const 48)) (i32.const 110) (i32.const 0))
    (call $gd_text (i32.const 40) (i32.const 208) (i32.const 640) (i32.const 0))
    (if (i32.eq (global.get $mode) (i32.const 2))
      (then
        (call $gd_text (i32.const 24) (i32.const 112) (i32.const 896) (i32.const 1))
        (call $gd_text (i32.const 24) (i32.const 148) (i32.const 960) (i32.const 0))
        (call $gd_text (i32.const 24) (i32.const 184) (i32.const 1024) (i32.const 0))))
    (if (i32.eq (global.get $mode) (i32.const 3))
      (then
        (call $gd_text (i32.const 24) (i32.const 112) (i32.const 1088) (i32.const 1))
        (call $gd_text (i32.const 24) (i32.const 148) (i32.const 1152) (i32.const 0)))) )

  (func $resetEnemy
    (local $max i32)
    (global.set $enemymax
      (i32.add
        (i32.mul (global.get $floor) (i32.const 10))
        (i32.const 30)))
    (global.set $enemyhp (global.get $enemymax)))

  (func $newFloor
    (global.set $floor (i32.add (global.get $floor) (i32.const 1)))
    (global.set $room (i32.const 1))
    (global.set $maxhp (i32.add (global.get $maxhp) (i32.const 8)))
    (global.set $hp (global.get $maxhp))
    (global.set $attack (i32.add (global.get $attack) (i32.const 2)))
    (global.set $potions (i32.add (global.get $potions) (i32.const 1)))
    (call $resetEnemy))

  (func $attackEnemy
    (local $counter i32)
    (global.set $enemyhp (i32.sub (global.get $enemyhp) (global.get $attack)))
    (if (i32.le_s (global.get $enemyhp) (i32.const 0))
      (then
        (global.set $enemyhp (i32.const 0))
        (global.set $wins (i32.add (global.get $wins) (i32.const 1)))
        (global.set $gold (i32.add (global.get $gold) (i32.mul (global.get $floor) (i32.const 18))))
        (global.set $xp (i32.add (global.get $xp) (i32.mul (global.get $floor) (i32.const 25))))
        (if (i32.ge_s (global.get $wins) (i32.const 3))
          (then
            (global.set $wins (i32.const 0))
            (if (i32.ge_s (global.get $floor) (i32.const 10))
              (then (global.set $mode (i32.const 3)))
              (else (call $newFloor))))))
      (else
        (local.set $counter (i32.add (i32.mul (global.get $floor) (i32.const 3)) (i32.const 4)))
        (global.set $hp (i32.sub (global.get $hp) (local.get $counter)))
        (if (i32.le_s (global.get $hp) (i32.const 0))
          (then (global.set $hp (i32.const 0)) (global.set $mode (i32.const 2)))))))

  (func $usePotion
    (if (i32.and (i32.gt_s (global.get $potions) (i32.const 0)) (i32.lt_s (global.get $hp) (global.get $maxhp)))
      (then
        (global.set $potions (i32.sub (global.get $potions) (i32.const 1)))
        (global.set $hp (i32.add (global.get $hp) (i32.const 38)))
        (if (i32.gt_s (global.get $hp) (global.get $maxhp)) (then (global.set $hp (global.get $maxhp)))))))

  (func $drawNumber (param $value i32) (param $x i32) (param $y i32)
    (if (i32.ge_s (local.get $value) (i32.const 1000))
      (then (call $gd_text (local.get $x) (local.get $y) (i32.const 16) (i32.const 0)))) )

  (func $renderDungeon
    (call $gd_rect (i32.const 12) (i32.const 12) (call $gd_width) (i32.const 72) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 26) (i32.const 16) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 58) (i32.const 320) (i32.const 1))
    (call $gd_text (i32.const 24) (i32.const 112) (i32.const 256) (i32.const 0))

    (call $gd_rect (i32.const 24) (i32.const 152) (i32.sub (call $gd_width) (i32.const 48)) (i32.const 82) (i32.const 0))
    (call $gd_text (i32.const 40) (i32.const 168) (i32.const 320) (i32.const 1))

    (call $gd_rect (i32.const 24) (i32.const 252) (i32.sub (call $gd_width) (i32.const 48)) (i32.const 54) (i32.const 0))
    (call $gd_text (i32.const 40) (i32.const 268) (i32.const 640) (i32.const 0))

    (call $gd_rect (i32.const 24) (i32.const 326) (i32.sub (call $gd_width) (i32.const 48)) (i32.const 96) (i32.const 0))
    (call $gd_rect (i32.const 40) (i32.const 342) (i32.const 100) (i32.const 24) (i32.const 1))
    (call $gd_rect (i32.const 155) (i32.const 342) (i32.const 100) (i32.const 24) (i32.const 0))
    (call $gd_rect (i32.const 270) (i32.const 342) (i32.const 100) (i32.const 24) (i32.const 0)) )

  ;; Render dispatch is kept in gd_render below.
  (func $gd_render_dispatch (export "gd_render")
    (if (i32.eq (global.get $mode) (i32.const 0))
      (then (call $title))
      (else (call $renderDungeon)))

  (func (export "gd_input") (param $code i32) (param $x i32) (param $y i32) (result i32)
    (if (i32.eq (global.get $mode) (i32.const 0))
      (then (if (i32.eq (local.get $code) (i32.const 5)) (then (global.set $mode (i32.const 1)) (call $resetEnemy))) (return (i32.const 1)))
    (if (i32.eq (global.get $mode) (i32.const 2))
      (then
        (if (i32.eq (local.get $code) (i32.const 5))
          (then
            (global.set $mode (i32.const 1)) (global.set $floor (i32.const 1)) (global.set $room (i32.const 1))
            (global.set $hp (i32.const 100)) (global.set $maxhp (i32.const 100)) (global.set $attack (i32.const 14))
            (global.set $gold (i32.const 0)) (global.set $xp (i32.const 0)) (global.set $potions (i32.const 3))
            (global.set $wins (i32.const 0)) (call $resetEnemy)))
        (return (i32.const 1)))
    (if (i32.eq (global.get $mode) (i32.const 3))
      (then (return (i32.const 0))) )

    (if (i32.eq (local.get $code) (i32.const 1))
      (then (global.set $room (i32.add (global.get $room) (i32.const 1)))) )
    (if (i32.eq (local.get $code) (i32.const 2))
      (then (if (i32.gt_s (global.get $room) (i32.const 1)) (then (global.set $room (i32.sub (global.get $room) (i32.const 1)))))))
    (if (i32.eq (local.get $code) (i32.const 4))
      (then (call $attackEnemy)))
    (if (i32.eq (local.get $code) (i32.const 3))
      (then (call $usePotion)))
    (if (i32.eq (local.get $code) (i32.const 5))
      (then
        (if (i32.eq (global.get $enemyhp) (i32.const 0))
          (then (call $resetEnemy))
          (else (call $attackEnemy)))) )
    (return (i32.const 1)))
)
