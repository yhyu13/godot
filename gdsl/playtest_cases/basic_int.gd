@tool
extends Node

func _ready():
	var p = Player.new()
	print("initial hp=", p.hp)                     # expect 3 (default)
	p.take_damage()                                 # rule method (TakeDamage -> take_damage)
	print("after_take_damage hp=", p.hp)            # expect 2
	var ok = (p.hp == 2)
	print("PASS basic_int hp 3->2" if ok else "FAIL basic_int: hp expected 2 got %d" % p.hp)
	get_tree().quit()
