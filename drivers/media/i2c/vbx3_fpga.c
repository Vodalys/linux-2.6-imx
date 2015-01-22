/*
 * VBX3 FPGA i2c driver.
 * Copyright (C) 2014  Jean-Michel Hautbois
 *
 * A/V source switching Vodalys VBX3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <linux/regmap.h>

MODULE_DESCRIPTION("i2c device driver for VBX3 fpga source switch");
MODULE_AUTHOR("Jean-Michel Hautbois");
MODULE_LICENSE("GPL");

#define	VBX3_FPGA_REG_VERSION			0x00
#define	VBX3_FPGA_REG_CTRL_CHAN0		0x01
#define	VBX3_FPGA_REG_CTRL_CHAN1		0x02
#define	VBX3_FPGA_REG_CTRL_PATTERN_CHAN0	0x03
#define	VBX3_FPGA_REG_GLOBAL_STATUS		0x04
#define	VBX3_FPGA_REG_STATUS_SDI0		0x05
#define	VBX3_FPGA_REG_STATUS_SDI1		0x06
#define	VBX3_FPGA_REG_EVENT_CHAN0		0x07
#define	VBX3_FPGA_REG_EVENT_CHAN1		0x08

enum vbx3_fpga_input_pads {
	VBX3_FPGA_INPUT_SDI0 = 0,
	VBX3_FPGA_INPUT_ADV7611_HDMI,
	VBX3_FPGA_INPUT_SDI1,
	VBX3_FPGA_INPUT_ADV7604_HDMI,
};
enum vbx3_fpga_output_pads {
	VBX3_FPGA_OUTPUT_CHANNEL0 = 4,
	VBX3_FPGA_OUTPUT_CHANNEL1,
};

#define	VBX3_FPGA_PADS_INPUT_NUM	4
#define	VBX3_FPGA_PADS_OUTPUT_NUM	2
#define	VBX3_FPGA_PADS_NUM		VBX3_FPGA_PADS_INPUT_NUM+VBX3_FPGA_PADS_OUTPUT_NUM

struct vbx3_fpga_state {
	struct v4l2_subdev sd;
	struct media_pad pads[VBX3_FPGA_PADS_NUM];
	struct i2c_client *i2c_client;
	struct regmap *regmap;
};

static const struct regmap_config vbx3_fpga_regmap = {
	.name			= "vbx3_fpga",
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= 0xff,
	.cache_type		= REGCACHE_NONE,
};

static inline struct vbx3_fpga_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct vbx3_fpga_state, sd);
}

static int vbx3_fpga_s_routing (struct v4l2_subdev *sd,
				u32 input, u32 output, u32 config)
{
	struct vbx3_fpga_state *state = to_state(sd);
	return 0;
}

static int vbx3_fpga_log_status(struct v4l2_subdev *sd)
{
	struct vbx3_fpga_state *state = to_state(sd);
	unsigned int value;

	static const char * const sdi_format[] = {
		"SD",
		"HD-SDI",
		"3G-SDI",
	};
	static const char * const sdi_video[] = {
		"720x576",
		"1280x720",
		"1920x1035",
		"1920x1080",
	};
	static const char * const sdi_fps[] = {
		"undefined",
		"24p",
		"25p",
		"30p",
		"50i",
		"60i",
		"50p",
		"60p",
	};
	static const char * const chan_audio[] = {
		"HDMI",
		"SDI",
		"sgtl5000",
		"sgtl5000",
	};
	static const char * const chan_video[] = {
		"HDMI",
		"SDI",
	};

	v4l2_info(sd, "-----Chip status-----\n");

	regmap_read(state->regmap, VBX3_FPGA_REG_VERSION, &value);
	v4l2_info(sd, "FPGA version: 0x%2x\n", value);

	regmap_read(state->regmap, VBX3_FPGA_REG_GLOBAL_STATUS, &value);
	v4l2_info(sd, "HDMI 0 connected : %s\n", (value & 0x80) ? "Yes" : "No");
	v4l2_info(sd, "HDMI 1 connected : %s\n", (value & 0x40) ? "Yes" : "No");
	regmap_read(state->regmap, VBX3_FPGA_REG_STATUS_SDI0, &value);
	v4l2_info(sd, "SDI 0 locked : %s\n", (value & 0x80) ? "Yes" : "No");
	if (value & 0x80)
		v4l2_info(sd, "SDI 0 format %s: %s@%s\n", sdi_format[(value & 0x60) >> 5],
						sdi_video[(value & 0x18) >> 3],
						sdi_fps[value & 0x07]);

	regmap_read(state->regmap, VBX3_FPGA_REG_STATUS_SDI1, &value);
	v4l2_info(sd, "SDI 1 locked : %s\n", (value & 0x80) ? "Yes" : "No");
	if (value & 0x80)
		v4l2_info(sd, "SDI 1 format %s: %s@%s\n", sdi_format[(value & 0x60) >> 5],
						sdi_video[(value & 0x18) >> 3],
						sdi_fps[value & 0x07]);
	v4l2_info(sd, "-----Control channels-----\n");
	regmap_read(state->regmap, VBX3_FPGA_REG_CTRL_CHAN0, &value);
	v4l2_info(sd, "Channel 0 : Audio %s, Video %s\n", chan_audio[(value & 0x06)>>1],
							  chan_video[(value & 0x01)]);
	regmap_read(state->regmap, VBX3_FPGA_REG_CTRL_CHAN1, &value);
	v4l2_info(sd, "Channel 1 : Audio %s, Video %s\n", chan_audio[(value & 0x06)>>1],
							  chan_video[(value & 0x01)]);
	return 0;
};

/*
 * find_link_by_sinkpad_index - Return a link of an entity where the sink
 * corresponds to the same entity and the index is index
 */
static struct media_link *find_link_by_sinkpad_index(struct media_entity *entity,
						     unsigned int index)
{
	int i = 0;
	struct media_link *result = NULL;

	while (!result && i < entity->num_links) {

		struct media_link *link = &entity->links[i];

		if (link && (link->sink->entity->id == entity->id) &&
				(link->sink->index == index))
			result = link;

		i++;
	}

	return result;
}

/*
 * vbx3_fpga_link_setup - Setup VBX3_FPGA connections.
 * @entity : Pointer to media entity structure
 * @local  : Pointer to local pad array
 * @remote : Pointer to remote pad array
 * @flags  : Link flags
 * return -EINVAL or zero on success
 */
static int vbx3_fpga_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct vbx3_fpga_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct media_link *link;
	unsigned int value;

	if (!flags){
		dev_dbg(&client->dev, "Deactivating link from %s to %s\n",
				 remote->entity->name,
				 local->entity->name);
		return 0;
	}

	switch (local->index){
	case VBX3_FPGA_INPUT_ADV7611_HDMI:

		link = find_link_by_sinkpad_index(entity,
				VBX3_FPGA_INPUT_SDI0);

		/* Check if VBX3_FPGA_INPUT_SDI0 is activated */
		if (link && link->flags == MEDIA_LNK_FL_ENABLED) {
			dev_dbg(&client->dev,
				"You must first deactivate link with %s\n",
				link->source->entity->name);
			return -EINVAL;
		}
		regmap_read(state->regmap, VBX3_FPGA_REG_CTRL_CHAN0, &value);
		regmap_write(state->regmap,
			     VBX3_FPGA_REG_CTRL_CHAN0,
			     value & 0xfe);
		break;
	case VBX3_FPGA_INPUT_SDI0:

		link = find_link_by_sinkpad_index(entity,
				VBX3_FPGA_INPUT_ADV7611_HDMI);

		/* Check if VBX3_FPGA_INPUT_ADV7611_HDMI is activated */
		if (link && link->flags == MEDIA_LNK_FL_ENABLED) {
			dev_dbg(&client->dev,
				"You must first deactivate link with %s\n",
				link->source->entity->name);
			return -EINVAL;
		}

		regmap_read(state->regmap, VBX3_FPGA_REG_CTRL_CHAN0, &value);
		regmap_write(state->regmap,
			     VBX3_FPGA_REG_CTRL_CHAN0,
			     value | 0x01);
		break;
	case VBX3_FPGA_INPUT_SDI1:

		link = find_link_by_sinkpad_index(entity,
				VBX3_FPGA_INPUT_ADV7604_HDMI);

		/* Check if VBX3_FPGA_INPUT_ADV7604_HDMI is activated */
		if (link && link->flags == MEDIA_LNK_FL_ENABLED) {
			dev_dbg(&client->dev,
				"You must first deactivate link with %s\n",
				link->source->entity->name);
			return -EINVAL;
		}

		regmap_read(state->regmap, VBX3_FPGA_REG_CTRL_CHAN1, &value);
		regmap_write(state->regmap,
			     VBX3_FPGA_REG_CTRL_CHAN1,
			     value | 0X01);
		break;
	case VBX3_FPGA_INPUT_ADV7604_HDMI:

		link = find_link_by_sinkpad_index(entity,
				VBX3_FPGA_INPUT_SDI1);

		/* Check if VBX3_FPGA_INPUT_SDI1 is activated */
		if (link && link->flags == MEDIA_LNK_FL_ENABLED) {
			dev_dbg(&client->dev,
				"You must first deactivate link with %s\n",
				link->source->entity->name);
			return -EINVAL;
		}

		regmap_read(state->regmap, VBX3_FPGA_REG_CTRL_CHAN1, &value);
		regmap_write(state->regmap,
			     VBX3_FPGA_REG_CTRL_CHAN1,
			     value & 0xfe);
		break;
	case VBX3_FPGA_OUTPUT_CHANNEL0:
	case VBX3_FPGA_OUTPUT_CHANNEL1:
		break;
	default:
		dev_dbg(&client->dev, "Changing to unknown pad %d\n", local->index);
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int vbx3_fpga_g_register(struct v4l2_subdev *sd,
					struct v4l2_dbg_register *reg)
{
	int ret;
	struct vbx3_fpga_state *state = to_state(sd);

	regmap_read(state->regmap, reg->reg, &ret);
	if (ret < 0) {
		v4l2_info(sd, "Register %03llx not supported\n", reg->reg);
		return ret;
	}

	reg->size = 1;
	reg->val = ret;

	return 0;
}

static int vbx3_fpga_s_register(struct v4l2_subdev *sd,
					struct v4l2_dbg_register *reg)
{
	int ret;
	struct vbx3_fpga_state *state = to_state(sd);

	ret = regmap_write(state->regmap, reg->reg, reg->val);
	if (ret < 0) {
		v4l2_info(sd, "Register %03llx not supported\n", reg->reg);
		return ret;
	}

	return 0;
}
#endif

static const struct v4l2_subdev_video_ops vbx3_fpga_video_ops = {
	.s_routing = vbx3_fpga_s_routing,
};

static const struct v4l2_subdev_core_ops vbx3_fpga_core_ops = {
	.log_status = vbx3_fpga_log_status,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = vbx3_fpga_g_register,
	.s_register = vbx3_fpga_s_register,
#endif
};

static const struct v4l2_subdev_ops vbx3_fpga_ops = {
	.core = &vbx3_fpga_core_ops,
	.video = &vbx3_fpga_video_ops,
};

/* media operations */
static const struct media_entity_operations vbx3_fpga_media_ops = {
	.link_setup = vbx3_fpga_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

/* i2c implementation */

static int vbx3_fpga_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct vbx3_fpga_state *state;
	int version, status;
	int ret = 0;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);

	state = devm_kzalloc(&client->dev, sizeof(*state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	state->i2c_client = client;

	/* Configure regmap */
	state->regmap =	devm_regmap_init_i2c(state->i2c_client,
					&vbx3_fpga_regmap);
	if (IS_ERR(state->regmap)) {
		ret = PTR_ERR(state->regmap);
		v4l_err(state->i2c_client,
			"Error initializing regmap with error %d\n",
			ret);
		devm_kfree(&client->dev, state);
		return -EINVAL;
	}

	regmap_read(state->regmap, VBX3_FPGA_REG_VERSION, &version);
	if (version < 0) {
		v4l_err(client, "could not get version of FPGA\n");
		return -EPROBE_DEFER;
	} else {
		v4l_info(client, "version read : 0x%x\n", version);
	}

	/* Set default Control Channel values */
	regmap_write(state->regmap, VBX3_FPGA_REG_CTRL_CHAN0, 0x00);
	regmap_write(state->regmap, VBX3_FPGA_REG_CTRL_CHAN1, 0x00);

	regmap_read(state->regmap, VBX3_FPGA_REG_GLOBAL_STATUS, &status);
	v4l_info(client, "Status : 0x%x\n", status);

	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, client, &vbx3_fpga_ops);
	strlcpy(sd->name, "VBX3 FPGA video switch", sizeof(sd->name));
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	state->pads[VBX3_FPGA_INPUT_SDI0].flags = MEDIA_PAD_FL_SINK;
	state->pads[VBX3_FPGA_INPUT_ADV7611_HDMI].flags = MEDIA_PAD_FL_SINK;
	state->pads[VBX3_FPGA_INPUT_SDI1].flags = MEDIA_PAD_FL_SINK;
	state->pads[VBX3_FPGA_INPUT_ADV7604_HDMI].flags = MEDIA_PAD_FL_SINK;

	state->pads[VBX3_FPGA_OUTPUT_CHANNEL0].flags = MEDIA_PAD_FL_SOURCE;
	state->pads[VBX3_FPGA_OUTPUT_CHANNEL1].flags = MEDIA_PAD_FL_SOURCE;

	/* Set entity operations */
	state->sd.entity.ops = &vbx3_fpga_media_ops;

	ret = media_entity_init(&state->sd.entity, VBX3_FPGA_PADS_NUM, state->pads, 0);
	if (ret < 0)
		v4l2_err(client, "media entity failed with error %d\n", ret);

	ret = v4l2_async_register_subdev(sd);
	if (ret < 0) {
		media_entity_cleanup(&state->sd.entity);
		return ret;
	}

	v4l_info(client, "device probed\n");

	return ret;
}

static int vbx3_fpga_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vbx3_fpga_state *state;

	sd = &state->sd;
	media_entity_cleanup(&sd->entity);
	v4l2_async_unregister_subdev(sd);
	v4l2_device_unregister_subdev(sd);
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id vbx3_fpga_id[] = {
	{ "vbx3_fpga", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, vbx3_fpga_id);

static struct i2c_driver vbx3_fpga_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "vbx3_fpga",
	},
	.probe		= vbx3_fpga_probe,
	.remove		= vbx3_fpga_remove,
	.id_table	= vbx3_fpga_id,
};

module_i2c_driver(vbx3_fpga_driver);
