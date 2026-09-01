@tool
extends Node

static var _p = null
static var _b = null

func _ready():
	_p = Player.new()
	_p.hp = 7
	_b = Bullet.new()
	_b.owner = _p
	print("before reload: hp=", _p.hp, " owner_set=", (_b.owner != null))
	var gm = Engine.get_singleton("GDExtensionManager")
	var status = gm.reload_extension("res://named.gdextension")
	print("reload status=", status)
	print("after reload: hp=", _p.hp, " owner_same=", (_b.owner == _p))
	get_tree().quit()
