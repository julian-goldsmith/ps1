#define MASK(data,mask)				(data&mask)

#define GPU_CONTROL_PORT	(*(int*)0x1f801814)
#define GPU_DATA_PORT		(*(int*)0x1f801810)

// commands sent to data port
#define GPU_CMD_MASK				0xff000000
#define GPU_CMD_POLY_3POINT_MONO	0x20000000
#define GPU_CMD_SET_DRAW_MODE		0xe1000000
#define GPU_CMD_SET_TEXTURE_WINDOW	0xe2000000
#define GPU_CMD_SET_DRAW_AREA_TOPL	0xe3000000
#define GPU_CMD_SET_DRAW_AREA_BOTR	0xe4000000
#define GPU_CMD_SET_DRAW_OFFSET		0xe5000000
#define GPU_CMD_SET_MASK			0xe6000000
#define GPU_CMD_RESET_GPU			0x00000000
#define GPU_CMD_DISPLAY_ENABLE		0x03000000
#define GPU_CMD_DISPLAY_DISABLE		0x03000001

// commands sent to control port
#define GPU_CMD_DISPLAY_MODE		0x08000000

// statuses read from control port
#define GPU_STAT_CMD_READY			(1<<0x1c)
#define GPU_STAT_BUSY				(1<<0x1a)

#define NULL				(void*)0x00000000

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef uint8_t		color_t[3];			// format: B,G,R
typedef uint16_t	vec2_t[2];			// format: X,Y
typedef uint16_t	vec3_t[3];			// format: X,Y,Z
typedef vec3_t		mat3_t[3];			// 3x3 matrix

typedef struct {
  mat3_t m;
  vec3_t t;
} MATRIX;

typedef struct
{
	color_t c;
	vec3_t p[3];
} tri_t;

#define MAX_POLYS 4000

unsigned long pad_buf = 0;
unsigned long pad_data = 0;

static int ntris = 1;
static tri_t tris[] =
{
	{
		{0x00, 0x00, 0xff},
		{
			{0x0000, 0x0007, 0x0007/* 0xfe00 */},
			{0x0000, 0x0000, 0x0007/* 0xfe00 */},
			{0x0007, 0x0000, 0x0007/* 0xfe00 */}
		}
	}
};

extern void printf(char * fmt, ...);
extern PAD_init(unsigned long nazo, unsigned long *pad_buf);

void gpu_set_status(int status)
{
	GPU_CONTROL_PORT = status;
}

void gpu_send_data(int data)
{
	GPU_DATA_PORT = data;
}

void gpu_draw_tri_mono(color_t color, vec2_t p0, vec2_t p1, vec2_t p2)
{
	int gpu_color = MASK(color[0],0x000000ff)<<16 | 		// the color in the gpu's format
					MASK(color[1],0x000000ff)<<8 | 
					MASK(color[2],0x000000ff);
	gpu_send_data(GPU_CMD_POLY_3POINT_MONO|MASK(gpu_color,0x00ffffff));
	gpu_send_data(MASK(p0[1],0x0000ffff)<<16|MASK(p0[0],0x0000ffff));
	gpu_send_data(MASK(p1[1],0x0000ffff)<<16|MASK(p1[0],0x0000ffff));
	gpu_send_data(MASK(p2[1],0x0000ffff)<<16|MASK(p2[0],0x0000ffff));
}

void gte_RTPS ( vec2_t *vert_out, vec3_t *vert_in, unsigned long n_vert, short * buffer_z )
{
	printf("in gte_RTPS\n");
	__asm__ volatile ("

		li		$2, 0
		cfc2	$31, $2
	
	    project_vertex:

		lwc2	$0, 0x0(%1)		# VXY0 = x/y of vertex
		lwc2	$1, 0x4(%1)		# VZ0 = z of vertex
	
		nop
		nop
		c2	0x0180001		# do the calculation (RTPS)

		mfc2	$2, $14			# $2 = SXYP (projected X/Y coordinates)
		mfc2	$8, $19			# $2 = SZ3 (projected Z coordinate)

		sra	$3, $2, 0x10		# v1 = Y
		andi	$2, $2, 0xffff		# v0 = X
		andi	$8, $8, 0xffff		# Z

		sh	$2, 0x0(%0)		# Store projected X
		sh	$3, 0x2(%0)		# Store projected Y
		sh	$8, 0x0(%3)		# Store projected Z

		addu	%0, %0, 0x4		# Next destination vertex
		addu	%1, %1, 0x8		# Next source vertex
		addu	%3, %3, 0x2		# Next source vertex

		sub	%2, %2, 1
		bnez	%2, project_vertex

		"
		:
		: "r"( vert_out ), "r"( vert_in ), "r"( n_vert), "r"( buffer_z )
		: "2", "3", "8"
		);

}

void gte_rtv0tr (vec3_t * vertex_out, vec3_t * vertex_in, unsigned long number_of_vertices)
{
	printf("in gte_rtv0tr\n");
	__asm__ volatile ("

	gte_rtv0tr_loop:
		lwc2	$0, 0(%1)		# VXY0 = x/y of vertex
		lwc2	$1, 4(%1)		# VZ0 = z of vertex

		nop
		nop
		c2	0x400012		# do the gte calculation (rtv0tr)

		mfc2	$8,  $9
		mfc2	$9,  $10
		mfc2	$10, $11
		sh	$8,  0(%0)		# vect_out.x = IR1
		sh	$9,  2(%0)		# vect_out.y = IR2
		sh	$10, 4(%0)		# vect_out.z = IR3

		addiu	%0, %0, 0x8		# Next destination vector
		addiu	%1, %1, 0x8		# Next source vector

		sub	%2, %2, 1
		bnez	%2, gte_rtv0tr_loop

		"
		:	
		: "r"( vertex_out ), "r"( vertex_in ), "r"( number_of_vertices)
		: "8", "9", "10"
		);
}

void gte_put_rotation_matrix (MATRIX * matrix)
{
	printf("in gte_put_rotation_matrix\n");

	asm volatile ("

		lw	$8,   0(%0)
		lw	$9,   4(%0)
		lw	$10,  8(%0)
		lw	$11, 12(%0)
		lw	$12, 16(%0)
		lw	$15, 20(%0)
		lw	$24, 24(%0)
		lw	$25, 28(%0)

		ctc2	$8,  $0			# R11R12 = matrix.m[0][0]/[0][1]
		ctc2	$9,  $1			# R13R21 = matrix.m[0][2]/[1][0]
		ctc2	$10, $2			# R22R23 = matrix.m[1][1]/[1][2]
		ctc2	$11, $3			# R31R32 = matrix.m[2][0]/[2][1]
		ctc2	$12, $4			# R33    = matrix.m[2][2]
		ctc2	$15, $5			# TRX = matrix.t[0]
		ctc2	$24, $6			# TRX = matrix.t[0]
		ctc2	$25, $7			# TRX = matrix.t[0]

		"
		:
		: "r"( &matrix->m[0] )
		: "8", "9", "10", "11", "12", "15", "24", "25"
		);
}

void GsResetMatrix(MATRIX *matrix)
{
	printf("in GsResetMatrix\n");
	
    matrix->m[0][0] = 1;
    matrix->m[0][1] = 0;
	matrix->m[0][2] = 0;
    matrix->m[1][0] = 0;
	matrix->m[1][1] = 1;
	matrix->m[1][2] = 0;
    matrix->m[2][0] = 0;
	matrix->m[2][1] = 0;
	matrix->m[2][2] = 1;
}

void GsSet3dOffset(int x, int y)
{
	printf("in GsSet3dOffset\n");
	
    asm volatile ("
	sll	%0, %0, 16		# Make 16.16 fixed point
	sll	%1, %1, 16
	ctc2	%0, $24			# OFX = (x << 16) (Screen offset X)
	ctc2	%1, $25			# OFY = (y << 16) (Screen offset Y)
    "
	:
	: "r" (x), "r" (y)
	);
}

void GsInit3d(void)
{
	printf("in GsInit3d\n");
	
    asm volatile ("
	lui	$4, 0x4000		# $4 = 0x40000000
	mfc0	$5, $12
	nop
	or	$5, $4			# enable the GTE
	mtc0	$5, $12
	nop

	addiu	$8, $0, 0x555		# ZSF3 - average scale factor (ONE/3)
	ctc2	$8, $29

	//addiu	$9, $0, 0x400		# ZSF4  - average scale factor (ONE/4)
	//ctc2	$9, $30

	addiu	$8, $0, 0x0200		# H - Projection plane distance
	ctc2	$8, $26

	addiu	$9, $0, 0xEF9E		# DQA  -Depth queuing parameter A (coefficient)
	ctc2	$9, $27			# 0xEF9E (fogging plane)

	addiu	$8, $0, 0x140		# DQB - Depth queuing parameter B (offset)
	ctc2	$8, $28			# 0x140 (fogging plane)
    ");

    GsSet3dOffset (0, 0);
}

static short r[3][3] =
{
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1}
};

static short TR[3] = {0, 0, 0};

const short H = 0x0200;
const short dqa = 0xEF9E;
const short dqb = 0x0140;
const short OFX = 0;
const short OFY = 0;

void rtps_software(uint16_t vx0, uint16_t vy0, uint16_t vz0, vec2_t outvec)
{
	int mac1 = TR[0] + r[0][0] * vx0 + r[0][1] * vy0 + r[0][2] * vz0;
	int mac2 = TR[1] + r[1][0] * vx0 + r[1][1] * vy0 + r[1][2] * vz0;
	int mac3 = TR[2] + r[2][0] * vx0 + r[2][1] * vy0 + r[2][2] * vz0;
	short ir1 = (short) mac1;
	short ir2 = (short) mac2;
	short ir3 = (short) mac3;
	int sx2 = OFX + ir1 * (H / ir3);
	int sy2 = OFY + ir2 * (H / ir3);
	
	outvec[0] = sx2;
	outvec[1] = sy2;
}

typedef unsigned long u64;
typedef long s64;

short FlimG1(s64 x) {
	return (x >  1023) ?  1023 :
		   (x < -1024) ? -1024 :
		   (short) x;
}

void gteRTPS(uint32_t VX0, uint32_t VY0, uint32_t VZ0, vec2_t SXY) {
	long IR1 = (uint32_t) (s64)(VX0)>>12;
	long IR2 = (uint32_t) (s64)(VY0)>>12;
	u64 SZ2 = (u64)(uint16_t) (s64)(VZ0)>>12;
	u64 FDSZ;
	
	printf("IR1 IR2 SZ2: %li %li %lu\n", IR1, IR2, SZ2);
	
	FDSZ = (SZ2 >> 16 == 0) ? 2 : (((u64)H) << 16) / (((u64)SZ2));
	printf("FDSZ: %lu\n", FDSZ);
	
	SXY[0] = /*FlimG1*/((IR1 << 16 * FDSZ) >> 16);
	SXY[1] = /*FlimG1*/((IR2 << 16 * FDSZ) >> 16);
}

void tri_draw(tri_t *tri)
{
	MATRIX rotate_matrix, camera_matrix;
	short buffer_z[3];
	vec2_t buffer_2d[3];
	
	printf("in tri_draw\n");
	
	GsResetMatrix(&rotate_matrix);
	GsResetMatrix(&camera_matrix);
	
	rotate_matrix.t[0] = 0;
	rotate_matrix.t[1] = 0;
	rotate_matrix.t[2] = 1;
	camera_matrix.t[0] = 0;
	camera_matrix.t[1] = 0;
	camera_matrix.t[2] = 10;

	/* rotate the object */
	gte_put_rotation_matrix (&rotate_matrix);
	//gte_rtv0tr (buffer_3d, tri->p, 3);
	printf("tri 1: %hd %hd %hd\n", tris->p[0][0], tris->p[0][1], tris->p[0][2]);
	printf("tri 2: %hd %hd %hd\n", tris->p[1][0], tris->p[1][1], tris->p[1][2]);
	printf("tri 3: %hd %hd %hd\n", tris->p[2][0], tris->p[2][1], tris->p[2][2]);
	
	gte_RTPS(&buffer_2d, tris->p/*buffer_3d*/, 3, &buffer_z);
	//gteRTPS(buffer_3d[0][0], buffer_3d[0][1], buffer_3d[0][2], buffer_2d[0]);
	//gteRTPS(buffer_3d[1][0], buffer_3d[1][1], buffer_3d[1][2], buffer_2d[1]);
	//gteRTPS(buffer_3d[2][0], buffer_3d[2][1], buffer_3d[2][2], buffer_2d[2]);
	
	//rtps_software(tris->p[0][0], tris->p[0][1], tris->p[0][2], buffer_2d[0]);
	//rtps_software(tris->p[1][0], tris->p[1][1], tris->p[1][2], buffer_2d[1]);
	//rtps_software(tris->p[2][0], tris->p[2][1], tris->p[2][2], buffer_2d[2]);
	
	printf("tri 1: %hu %hu\n", buffer_2d[0][0], buffer_2d[0][1]);
	printf("tri 2: %hu %hu\n", buffer_2d[1][0], buffer_2d[1][1]);
	printf("tri 3: %hu %hu\n", buffer_2d[2][0], buffer_2d[2][1]);
	
	gpu_draw_tri_mono(tri->c, buffer_2d[0], buffer_2d[1], buffer_2d[2]);
}

void WaitVSync(void)
{
	volatile unsigned long *p = (volatile unsigned long *) &pad_buf;
	*p = 0x0000;
	while (*p == 0x0000);
	pad_data = *p;
}

void clear_screen(unsigned char r, unsigned char g, unsigned char b, short width, short height)
{
	gpu_send_data(0x60 << 24 | b << 16 | g << 8 | r);
	gpu_send_data(0x00000000);
	gpu_send_data(((int) height) << 16 | width);
}

void main()
{
	gpu_send_data(GPU_CMD_RESET_GPU);							/* Reset the GPU */
	gpu_send_data(GPU_CMD_DISPLAY_DISABLE);						/* Disable the GPU so we can set it up */
	gpu_set_status(0);											/* Some kind of reset ? */
	gpu_set_status(0x03000000);									/* Display Mask (enable display) */
	gpu_set_status(0x06c60260);									/* Screen horizontal start/end (0/256) */
	gpu_set_status(0x07040010);									/* Screen vertical start/end (0/240) */
	gpu_send_data(GPU_CMD_SET_DRAW_MODE|0x400);					/* Draw on display area / dither off / tpage0 */
	gpu_send_data(GPU_CMD_SET_DRAW_AREA_TOPL|0x00);				/* Draw Area x, y (0,0) */
	gpu_send_data(GPU_CMD_SET_DRAW_AREA_BOTR|0x07ffff);			/* Draw Area w, h (1023,511) */
	gpu_send_data(GPU_CMD_SET_DRAW_OFFSET|0x00);				/* Draw Offset (0,0) */
	gpu_set_status(GPU_CMD_DISPLAY_MODE|0x00000000);			/* Display mode, 256x240/NTSC/noninterlaced */
	gpu_send_data(GPU_CMD_DISPLAY_ENABLE);						/* Re-enable the GPU */
	
	gpu_set_status(0x03000000);									/* Display Mask (enable display) */
	
	PAD_init (0x20000001, &pad_buf);
	
	GsInit3d();
	
	while(1)
	{
		/* FIXME: use VBLANKs */
		while(!(GPU_CONTROL_PORT & GPU_STAT_BUSY));
		while(!(GPU_CONTROL_PORT & GPU_STAT_CMD_READY));
		
		clear_screen(0, 0, 0, 1023, 511);
		tri_draw(&tris[0]);
		
		WaitVSync();
	};
}
