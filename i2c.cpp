#define APB1_CLOCK_FREQ 36

void init_i2c()
{
    // Program the peripheral input clock in I2C_CR2 Register in order to generate correct timings
    // our APB1 clock freq is 36MHz with 72MHz clock
    I2C1->CR2 = APB1_CLOCK_FREQ; // 0b100100; // Only set FREQ to 36MHz
    // Configure the clock control registers
    SET_BIT(I2C1->CCR, I2C_CCR_FS); // Set Fm mode I2C
    CLEAR_BIT(I2C1->CCR, I2C_CCR_DUTY); // Set Fm mode to t(low)/t(high) = 2
    I2C1->CCR |= 0x2D; // CCR for 400kHz SCL frequency for a FREQ of 36MHz
    // Configure the rise time register
    I2C1->TRISE = 10; // Needs to be 11 for a max rise time of around 300ns
    // Program the I2C_CR1 register to enable the peripheral
    I2C1->CR1 = 
        I2C_CR1_PE |
        I2C_CR1_ACK;
}

// Generates start and sends the slaves address
void i2c_gen_start(uint8_t slave_addr)
{
    // Set the START bit in the I2C_CR1 register to generate a Start condition
    SET_BIT(I2C1->CR1, I2C_CR1_START);

    // The master now waits for a read of the SR1 register followed by a
    // write in the DR register with the Slave address
    while(!READ_BIT(I2C1->SR1, I2C_SR1_SB)) {} // Wait until SB bit is set
    int sr1 = I2C1->SR1; // CHECK: useless????
    I2C1->DR = slave_addr;
    
    // After sending the slave address the master waits for a read of the
    // SR1 register followed by a read of the SR2 register for clearing ADDR
    while(!READ_BIT(I2C1->SR1, I2C_SR1_ADDR)) {} // Wait until ADDR bit is set
    sr1 = I2C1->SR1; // CHECK: useless too????
    int sr2 = I2C1->SR2;
}

// TODO: finish this
void i2c_write_register(uint8_t slave_addr, uint8_t reg, uint8_t value)
{
    // In order to write we have to
    // START - SLAVE ADDRESS - SLAVE ACK - REGISTER ADDRESS - SLAVE ACK - VALUE - SLAVE ACK - STOP

    i2c_gen_start(slave_addr | 0);
    I2C1->DR = reg;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_TXE)) {}// loop until the TxE bit is set
    I2C1->DR = value;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_TXE)) {}// loop until the TxE bit is set
    SET_BIT(I2C1->CR1, I2C_CR1_STOP); // And stop, TxE and BTF are cleared by hardware
}

uint8_t i2c_read_register(uint8_t slave_addr, uint8_t reg)
{
    int sr1, sr2;
    // In order to read a single byte from a register, we have to send a 
    // START condition with the slave address with the R/W bit set to 0, so
    // that we can write the address of the register we want to read
    // after that we have to generate another start sequence but read the data
    // that the slave returns. We have to generate a Master NACK and STOP after
    // receiving the byte
    //
    // The sequence of events is as follows:
    // START - SLAVE ADDRESS - SLAVE ACK - REGISTER ADDRESS - SLAVE ACK - REPEATED START - SLAVE ADDRESS - SLAVE ACK - REGISTER VALUE - MASTER NACK - STOP
    
    // First send the register to read
    i2c_gen_start(slave_addr | 0);

    // Write the register we want to read
    I2C1->DR = reg;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_TXE)) {}// loop until the TxE bit is set

    // Now read the value of the register
    // Procedure for reading one byte
    i2c_gen_start(slave_addr | 1);
    CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
    SET_BIT(I2C1->CR1, I2C_CR1_STOP);

    while(!READ_BIT(I2C1->SR1, I2C_SR1_RXNE)) {} // Wait for first byte
    return I2C1->DR;
}


