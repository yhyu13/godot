@tool
extends Node

func _ready():
	var b = Bullet.new()
	var p = Player.new()
	print("t0 p.hp=", p.hp, " b.damage=", b.damage)   # 3 / 1
	b.on_hit(p)                                        # cross-participant rule (target Player)
	print("t1 p.hp=", p.hp)                            # expect 2
	var ok = (p.hp == 2)
	print("PASS target p.hp 3->2" if ok else "FAIL target p.hp expected 2 got %d" % p.hp)
	get_tree().quit()
