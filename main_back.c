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
typedef unsigned int uin32_t;
typedef uint8_t		color_t[3];			// format: B,G,R
typedef uint16_t	vec2_t[2];			// format: X,Y
typedef uint16_t	vec3_t[4];			// format: X,Y,Z
typedef vec3_t		mat3_t[4];			// 3x3 matrix

typedef struct
{
	color_t c;
	vec3_t p[3];
} tri_t;

extern void printf(char * fmt, ...);
extern void WaitVSync(void);
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

void mat3_mat3_mul(mat3_t A, mat3_t B, mat3_t out)
{
	int i, j, r;
	
	for(i = 0; i < 3; i++)
	{
		for(j = 0; j < 3; j++)
		{
			for(r = 0; r < 3; r++)
			{
				out[i][j] += A[i][r]*B[r][j];
			}
		}
	}
}

void mat3_vec3_mul(mat3_t A, vec3_t B, vec3_t out)
{
	out[0] = A[0][0]*B[0]+A[0][1]*B[0]+A[0][2]*B[0];
	out[1] = A[1][0]*B[1]+A[1][1]*B[1]+A[1][2]*B[1];
	out[2] = A[2][0]*B[2]+A[2][1]*B[2]+A[2][2]*B[2];
}

void project(vec3_t in[3], vec2_t out[3])
{
	out[0][0] = ((int)(in[0][0]*1023)/in[0][2]);
	out[0][1] = ((int)(in[0][1]*511)/in[0][2]);
	
	out[1][0] = ((int)(in[1][0]*1023)/in[1][2]);
	out[1][1] = ((int)(in[1][1]*511)/in[1][2]);
	
	out[2][0] = ((int)(in[2][0]*1023)/in[2][2]);
	out[2][1] = ((int)(in[2][1]*511)/in[2][2]);
}

typedef struct {
  short x, y;
} SVECTOR2D;

typedef struct {
  short x, y, z, pad;
} SVECTOR3D;

typedef struct {
  short m[3][3];
  long t[3];
} MATRIX;

#define MAX_POLYS 4000
//=======================================================
SVECTOR3D buffer_norm[MAX_POLYS];
SVECTOR3D buffer_3d  [MAX_POLYS];
SVECTOR2D buffer_2d  [MAX_POLYS];
short     buffer_z   [MAX_POLYS];

void gte_RTPS ( SVECTOR2D * vert_out, SVECTOR3D * vert_in, unsigned long n_vert, short * buffer_z )
{
	printf("in gte_RTPS\n");
	__asm__ volatile ("

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
		: "2", "8"
		);

}

void gte_rtv0tr (SVECTOR3D * vertex_out, SVECTOR3D * vertex_in, unsigned long number_of_vertices)
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

static int ntris = 1;
static tri_t tris[] =
{
	{
		{0x00, 0x00, 0xff},
		{
			{0x0f0, 0x0f0, 0x0f0, 0x000},
			{0x00f, 0x00f, 0x0f0, 0x000},
			{0x0f0, 0x000, 0x0f0, 0x000}
		}
	}
};

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
	sll	$4, $4, 16		# Make 16.16 fixed point
	sll	$5, $5, 16
	ctc2	$4, $24			# OFX = (x << 16) (Screen offset X)
	ctc2	$5, $25			# OFY = (y << 16) (Screen offset Y)
    ");
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

	addiu	$8, $0, 0x1020		# H - Projection plane distance
	ctc2	$8, $26

	addiu	$9, $0, 0xEF9E		# DQA  -Depth queuing parameter A (coefficient)
	ctc2	$9, $27			# 0xEF9E (fogging plane)

	addiu	$8, $0, 0x140		# DQB - Depth queuing parameter B (offset)
	ctc2	$8, $28			# 0x140 (fogging plane)
    ");

    GsSet3dOffset (1023, 511);

    //GsResetMatrix (&camera_matrix);

}

void tri_draw(tri_t *tri)
{
	vec2_t tri_proj[3];
	//SVECTOR3D *bufferSpot = buffer_3d;
	//SVECTOR3D tri_svec;
	MATRIX rotate_matrix, camera_matrix;
	//SVECTOR3D* debugtrip;
	
	printf("in tri_draw\n");
	
	GsResetMatrix(&rotate_matrix);
	GsResetMatrix(&camera_matrix);
	
	rotate_matrix.t[0] = 0;
	rotate_matrix.t[1] = 0;
	rotate_matrix.t[2] = 0;
	camera_matrix.t[0] = 0;
	camera_matrix.t[1] = 0;
	camera_matrix.t[2] = 0;
	
	//debugtrip = (SVECTOR3D*)&rotate_matrix.m[0];
	printf("debugtrip 1: %hu %hu %hu\n", rotate_matrix.m[0][0], rotate_matrix.m[0][1], rotate_matrix.m[0][2]);
	printf("debugtrip 2: %hu %hu %hu\n", rotate_matrix.m[1][0], rotate_matrix.m[1][1], rotate_matrix.m[1][2]);
	printf("debugtrip 3: %hu %hu %hu\n", rotate_matrix.m[2][0], rotate_matrix.m[2][1], rotate_matrix.m[2][2]);

	/* rotate the object */
	gte_put_rotation_matrix (&rotate_matrix);
	gte_rtv0tr (buffer_3d, (SVECTOR3D*)&tri->p[0], 3);
	printf("tri 1: %hu %hu %hu\n", buffer_3d[0].x, buffer_3d[0].y, buffer_3d[0].z);
	printf("tri 2: %hu %hu %hu\n", buffer_3d[1].x, buffer_3d[1].y, buffer_3d[1].z);
	printf("tri 3: %hu %hu %hu\n", buffer_3d[2].x, buffer_3d[2].y, buffer_3d[2].z);

	/* apply the camera position */
	/*gte_put_rotation_matrix (&camera_matrix);
	gte_rtv0tr (buffer_3d, buffer_3d, 1);
	printf("tri 1: %hu %hu %hu\n", buffer_3d[0].x, buffer_3d[0].y, buffer_3d[0].z);
	printf("tri 2: %hu %hu %hu\n", buffer_3d[1].x, buffer_3d[1].y, buffer_3d[1].z);
	printf("tri 3: %hu %hu %hu\n", buffer_3d[2].x, buffer_3d[2].y, buffer_3d[2].z);*/
	
	//project(tri->p, tri_proj);
	gte_RTPS(buffer_2d, buffer_3d, 3, buffer_z);
	printf("tri 1: %hu %hu %hu\n", buffer_3d[0].x, buffer_3d[0].y, buffer_3d[0].z);
	printf("tri 2: %hu %hu %hu\n", buffer_3d[1].x, buffer_3d[1].y, buffer_3d[1].z);
	printf("tri 3: %hu %hu %hu\n", buffer_3d[2].x, buffer_3d[2].y, buffer_3d[2].z);
	
	tri_proj[0][0] = buffer_2d[0].x;
	tri_proj[0][1] = buffer_2d[0].y;
	tri_proj[1][0] = buffer_2d[1].x;
	tri_proj[1][1] = buffer_2d[1].y;
	tri_proj[2][0] = buffer_2d[2].x;
	tri_proj[2][1] = buffer_2d[2].y;
	printf("tri 1: %hu %hu\n", tri_proj[0][0], tri_proj[0][1]);
	printf("tri 2: %hu %hu\n", tri_proj[1][0], tri_proj[1][1]);
	printf("tri 3: %hu %hu\n", tri_proj[2][0], tri_proj[2][1]);
	
	gpu_draw_tri_mono(tri->c, tri_proj[0], tri_proj[1], tri_proj[2]);
	//gpu_draw_tri_mono(tri->c, tri->p[0], tri->p[1], tri->p[2]);
}

long vblank()
{
	printf("asdf\n");
	return 0;
}

unsigned long pad_buf = 0;
unsigned long pad_data = 0;

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
	
	printf("test\n");
	
	GsInit3d();
	
	while(1)
	{
		/* FIXME: use VBLANKs */
		while(!(GPU_CONTROL_PORT & GPU_STAT_BUSY));
		while(!(GPU_CONTROL_PORT & GPU_STAT_CMD_READY));
		
		clear_screen(0, 0, 0, 1023, 511);
		tri_draw(&tris[0]);
		printf("vblank\n");
		
		WaitVSync();
		
		printf("back to main loop\n");
		
		//vblank();
	};
}
