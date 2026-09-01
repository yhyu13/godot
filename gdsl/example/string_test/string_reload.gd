@tool
extends Node

func _ready():
	var p = Hero.new()
	p.hp = 7
	p.nickname = "aria"
	print("before reload: hp=", p.hp, " nickname=", p.nickname, " is_nick=(", p.nickname == "aria", ")")
	var gm = Engine.get_singleton("GDExtensionManager")
	var status = gm.reload_extension("res://string_fields.gdextension")
	print("reload status=", status)
	print("after reload: hp=", p.hp, " nickname=", p.nickname, " nick_same=(", p.nickname == "aria", ")")
	p.free()
	print("freed ok")
	get_tree().quit()
