@tool
extends Node

func _ready():
	var all_pass = true
	var owner = Player.new()
	owner.hp = 1.0
	var before = owner.hp
	owner.take_damage()
	var after = owner.hp
	var ok = (before - after) == 1
	print("PASS owner hp -1" if ok else "FAIL owner hp -1 expect owner hp -1 got %s" % str(after))
	all_pass = all_pass and ok
	print("RESULT ALLPASS" if all_pass else "RESULT SOME_FAIL")
	get_tree().quit()

