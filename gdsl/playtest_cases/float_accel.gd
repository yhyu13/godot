@tool
extends Node

func _ready():
	var p = Player.new()
	print("initial speed=", p.speed, " hp=", p.hp)    # 300.4 / 3
	p.accel()                                          # rule method (Accel -> accel)
	print("after_accel speed=", p.speed)               # expect 305.4
	var ok = (abs(p.speed - 305.4) < 0.001)
	print("PASS float_accel speed 300.4->305.4" if ok else "FAIL float_accel speed expected 305.4 got %s" % str(p.speed))
	get_tree().quit()
